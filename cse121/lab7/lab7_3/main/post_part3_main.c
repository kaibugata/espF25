#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "protocol_examples_common.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"

#include "driver/i2c_master.h"

/**********************
 *  HTTP DEFINITIONS
 **********************/
#define WEB_SERVER "192.168.0.104" //change
#define WEB_PORT "8000" //change
#define WEB_PATH "/"

static const char *TAG = "POST_TEMP";

/* Replaced const payload with buffer */
static char post_payload[256];

static const char *REQUEST_TEMPLATE = 
    "POST " WEB_PATH " HTTP/1.0\r\n"
    "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
    "User-Agent: esp-idf/1.0 esp32 curl\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: %d\r\n"
    "\r\n"
    "%s";

/**********************
 *  SHTC3 DEFINITIONS
 **********************/
#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY
#define I2C_MASTER_TIMEOUT_MS       1000
#define SHTC3_ADDR                  0x70

static i2c_master_dev_handle_t shtc3_handle;

/***************
 * I2C helpers
 ***************/
static esp_err_t shtc3_write16(i2c_master_dev_handle_t dev, uint16_t data)
{
    uint8_t buf[2] = { data >> 8, data & 0xFF };
    return i2c_master_transmit(dev, buf, 2, I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t shtc3_read(i2c_master_dev_handle_t dev, uint16_t *rh, uint16_t *temp)
{
    uint8_t buf[6] = {0};
    ESP_ERROR_CHECK(i2c_master_receive(dev, buf, 6, I2C_MASTER_TIMEOUT_MS));

    *rh   = (buf[0] << 8) | buf[1];
    *temp = (buf[3] << 8) | buf[4];
    return ESP_OK;
}

static void i2c_master_init_all(void)
{
    i2c_master_bus_handle_t bus;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &shtc3_handle));

    ESP_LOGI(TAG, "I2C + SHTC3 initialized");
}

/**********************
 * Read temperature
 **********************/
float read_shtc3_temp(void)
{
    uint16_t rh_raw, t_raw;

    shtc3_write16(shtc3_handle, 0x3517); // wake
    vTaskDelay(pdMS_TO_TICKS(100));

    shtc3_write16(shtc3_handle, 0x5C24); // measure
    vTaskDelay(pdMS_TO_TICKS(250));

    shtc3_read(shtc3_handle, &rh_raw, &t_raw);

    shtc3_write16(shtc3_handle, 0xB098); // sleep

    float tempC = -45 + (175 * t_raw) / (1 << 16);
    return tempC;
}

//HELPER FUNCTION!!!!

static bool http_raw_get(const char *host, const char *port, const char *path,
                         char *response, size_t max_len)
{
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0 || !res) return false;

    int s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) { freeaddrinfo(res); return false; }

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
        close(s);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    char req[256];
    int req_len = snprintf(req, sizeof(req),
                           "GET %s HTTP/1.0\r\n"
                           "Host: %s\r\n"
                           "User-Agent: esp32 curl\r\n"
                           "\r\n",
                           path, host);

    send(s, req, req_len, 0);

    int r, total = 0;
    while ((r = recv(s, response + total, max_len - total - 1, 0)) > 0) {
        total += r;
        if (total >= max_len - 1) break;
    }
    response[total] = '\0';

    close(s);
    return true;
}


static char* extract_json(char *resp)
{
    char *json = strchr(resp, '{');
    if (!json) return NULL;
    return json;
}


static bool parse_city(char *json, char *city_out, size_t city_sz)
{
    char *p = strstr(json, "\"city\"");
    if (!p) return false;

    p = strchr(p, ':');
    if (!p) return false;
    p++;

    while (*p == ' ' || *p == '\"') p++;

    char *end = strchr(p, '\"');
    if (!end) return false;

    size_t len = end - p;
    if (len >= city_sz) len = city_sz - 1;

    memcpy(city_out, p, len);
    city_out[len] = '\0';
    return true;
}


static bool parse_temp(char *json, float *tempC)
{

    if(!json || !tempC) return false;

    char *key = strstr(json, "\"temp_C\"");
    if (!key) return false;

    char *p = strchr(key, ':');
    if (!p) return false;
    p++;

    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '\"')
	    p++;

   char numbuf[32];
   size_t ni = 0;

   while(*p != '\0' && ni + 1 < sizeof(numbuf)){
   	if((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E'){
		numbuf[ni++] = *p;
		p++;
	
	} else {
		break;
	}
   }
   numbuf[ni] = '\0';

   if (ni == 0){
   	ESP_LOGE(TAG, "parse_temp: no numeric chars afeter temp_C");
	return false;
   }

   ESP_LOGI(TAG, "parse_temp: numeric token = '%s'", numbuf);

    *tempC = atof(numbuf);
    return true;
}


static void urlencode_spaces(const char *in, char *out, size_t out_sz){
	size_t j = 0;
	for(size_t i = 0; in[i] != 0 && j < out_sz - 1; i++){
		if (in[i] == ' ') {
			if(j + 3 >= out_sz) break;
			out[j++] = '%';
			out[j++] = '2';
			out[j++] = '0';
		} else {
			out[j++] = in[i];
		
		}
	}
	out[j] = 0;
	

	}




//END OF Helper Functions




/**********************
 * HTTP POST TASK
 **********************/
static void http_post_task(void *pvParameters)
{
    const char *server_ip = WEB_SERVER;  
    const char *server_port = WEB_PORT;

    static char response[8192];
    char city[64];
    float outdoor_temp = 0.0;

    while (1)
    {
        /****************************************************
         * STEP 1: GET /location from your Python server
         ****************************************************/
        memset(response, 0, sizeof(response));

        if (!http_raw_get(server_ip, server_port, "/location",
                          response, sizeof(response))) {
            ESP_LOGE(TAG, "GET /location failed");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        char *json = extract_json(response);
        if (!json || !parse_city(json, city, sizeof(city))) {
            ESP_LOGE(TAG, "Could not parse city");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Server city: %s", city);

        /****************************************************
         * STEP 2: GET outdoor temperature from wttr.in
         ****************************************************/
        char encoded_city[64];
	urlencode_spaces(city, encoded_city, sizeof(encoded_city));


	char wttr_path[128];
        snprintf(wttr_path, sizeof(wttr_path), "/%s?format=j1&force=1",encoded_city);

        memset(response, 0, sizeof(response));

	ESP_LOGE(TAG, "requesting wttr path: %s", wttr_path);//testing

        if (!http_raw_get("wttr.in", "80", wttr_path,
                          response, sizeof(response))) {
            ESP_LOGE(TAG, "GET wttr.in failed");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

	ESP_LOGE(TAG, "Raw wttr.in response: \n%s", response);//testing

        json = extract_json(response);
        if (!json || !parse_temp(json, &outdoor_temp)) {
            ESP_LOGE(TAG, "Failed to parse outdoor temp");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Outdoor Temp: %.2f C", outdoor_temp);

        /****************************************************
         * STEP 3: Read ESP32 sensor temperature
         ****************************************************/
        float sensor_temp = read_shtc3_temp();
        ESP_LOGI(TAG, "Sensor Temp: %.2f C", sensor_temp);

        /****************************************************
         * STEP 4: Build JSON payload for POST
         ****************************************************/
        snprintf(post_payload, sizeof(post_payload),
                 "{\"city\":\"%s\",\"outdoor_temp_C\":%.2f,\"sensor_temp_C\":%.2f}",
                 city, outdoor_temp, sensor_temp);

        ESP_LOGI(TAG, "POST payload: %s", post_payload);

        /****************************************************
         * STEP 5: RAW SOCKET POST (your original code)
         ****************************************************/
        struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
        struct addrinfo *res;
        char recv_buf[128];

        int err = getaddrinfo(server_ip, server_port, &hints, &res);
        if (err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        int s = socket(res->ai_family, res->ai_socktype, 0);
        if (s < 0) {
            freeaddrinfo(res);
            continue;
        }

        if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            close(s);
            freeaddrinfo(res);
            continue;
        }
        freeaddrinfo(res);

        char request[512];
        int request_len = snprintf(request, sizeof(request),
                                   REQUEST_TEMPLATE,
                                   strlen(post_payload),
                                   post_payload);

        send(s, request, request_len, 0);

        int r;
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = recv(s, recv_buf, sizeof(recv_buf) - 1, 0);
            if (r > 0) printf("%s", recv_buf);
        } while (r > 0);

        close(s);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


/**********************
 * MAIN
 **********************/
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    i2c_master_init_all();

    xTaskCreate(&http_post_task, "http_post_task", 4096, NULL, 5, NULL);
}
