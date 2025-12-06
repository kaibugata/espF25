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
static char post_payload[64];

static const char *REQUEST_TEMPLATE = 
    "POST " WEB_PATH " HTTP/1.0\r\n"
    "Host: " WEB_SERVER ":" WEB_PORT "\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
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
    vTaskDelay(pdMS_TO_TICKS(400));

    shtc3_read(shtc3_handle, &rh_raw, &t_raw);

    shtc3_write16(shtc3_handle, 0xB098); // sleep
    
    float tempC = -45 + (175 * t_raw)/(1 << 16);
    //return -45 + (175.0f * t_raw) / 65536.0f;
    return tempC;
}

/**********************
 * HTTP POST TASK
 **********************/
static void http_post_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    char recv_buf[128];

    while (1)
    {
        /***********************************************
         * ONLY REQUIRED CHANGE #1:
         * Read temperature and format POST payload
         ***********************************************/
        float temp = read_shtc3_temp();
        snprintf(post_payload, sizeof(post_payload), "%.2f", temp);

        ESP_LOGI(TAG, "POSTING temp: %s C", post_payload);

        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
        if (err != 0 || res == NULL)
        {
            ESP_LOGE(TAG, "DNS lookup failed");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        int s = socket(res->ai_family, res->ai_socktype, 0);
        if (s < 0)
        {
            freeaddrinfo(res);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        if (connect(s, res->ai_addr, res->ai_addrlen) != 0)
        {
            close(s);
            freeaddrinfo(res);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        freeaddrinfo(res);

        char request[512];

        /***********************************************
         * ONLY REQUIRED CHANGE #2:
         * Use post_payload instead of POST_PAYLOAD
         ***********************************************/
        int request_len = snprintf(request, sizeof(request),
                                   REQUEST_TEMPLATE,
                                   strlen(post_payload),
                                   post_payload);

        send(s, request, request_len, 0);

        int r;
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = recv(s, recv_buf, sizeof(recv_buf)-1, 0);
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
