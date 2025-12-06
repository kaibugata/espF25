/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <unistd.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_rom_sys.h"


/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO_OUTPUT_IO_0: output
 * GPIO_OUTPUT_IO_1: output
 * GPIO_INPUT_IO_0:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO_INPUT_IO_1:  input, pulled up, interrupt from rising edge.
 *
 * Note. You can check the default GPIO pins to be used in menuconfig, and the IOs can be changed.
 *
 * Test:
 * Connect GPIO_OUTPUT_IO_0 with GPIO_INPUT_IO_0
 * Connect GPIO_OUTPUT_IO_1 with GPIO_INPUT_IO_1
 * Generate pulses on GPIO_OUTPUT_IO_0/1, that triggers interrupt on GPIO_INPUT_IO_0/1
 *
 */

#define GPIO_OUTPUT_IO_0    CONFIG_GPIO_OUTPUT_0
#define GPIO_OUTPUT_IO_1    CONFIG_GPIO_OUTPUT_1
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
/*
 * Let's say, GPIO_OUTPUT_IO_0=18, GPIO_OUTPUT_IO_1=19
 * In binary representation,
 * 1ULL<<GPIO_OUTPUT_IO_0 is equal to 0000000000000000000001000000000000000000 and
 * 1ULL<<GPIO_OUTPUT_IO_1 is equal to 0000000000000000000010000000000000000000
 * GPIO_OUTPUT_PIN_SEL                0000000000000000000011000000000000000000
 * */
#define GPIO_INPUT_IO_0     CONFIG_GPIO_INPUT_0
#define GPIO_INPUT_IO_1     CONFIG_GPIO_INPUT_1
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
/*
 * Let's say, GPIO_INPUT_IO_0=4, GPIO_INPUT_IO_1=5
 * In binary representation,
 * 1ULL<<GPIO_INPUT_IO_0 is equal to 0000000000000000000000000000000000010000 and
 * 1ULL<<GPIO_INPUT_IO_1 is equal to 0000000000000000000000000000000000100000
 * GPIO_INPUT_PIN_SEL                0000000000000000000000000000000000110000
 * */

#define TRIG_PIN	3
#define ECHO_PIN 	1



static const char *TAG = "example";

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

//MY DEFINITIONS:
#define SHTC3_ADDR	0x70	/*!< Address of the SHTC3 Sensor */
//END OF MY DEFINITIONS


//MY FUNCTION: WRITE A BYTE TO A SHCTC3 sensor
static esp_err_t shct3_register_write_byte(i2c_master_dev_handle_t dev_handle, uint16_t data)//data is 16 bits
{
	uint8_t write_buf[2] = {((data&0xff00)>>8),(data&0xff)};//concatenate data
	//ESP_LOGI(TAG, "write bruf done");
	esp_err_t err;
	err =  i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf),I2C_MASTER_TIMEOUT_MS);
	//ESP_LOGI(TAG, "master transmit done");
	return err;
}

//MY FUNCTION: READ A SEQUENCE OF BYTES FROM A SHCT3 SENSOR
static esp_err_t shct3_register_read(i2c_master_dev_handle_t dev_handle, uint16_t *humidity, uint16_t *temp)
{
	uint8_t read_buf[6] = {0}; //16'b humidity, 8'b h_checksom, 16'b temp, 8'b t_checksum
	//ESP_LOGI(TAG, "read buf init");
	esp_err_t err;
	err =  i2c_master_receive(dev_handle,read_buf ,sizeof(read_buf) ,I2C_MASTER_TIMEOUT_MS);//testing using master_receive instead of master_transmit_receive
	//ESP_LOGI(TAG, "err def");

	*humidity  = (read_buf[0]  << 8) | (read_buf[1]);
	*temp = (read_buf[3] << 8) | (read_buf[4]);
       	//ESP_LOGI(TAG, "humidity and temp done");
	//(a << 8) | b

	return err;	
}



/**
 * @brief i2c master initialization
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_ADDR, //modified
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}


float measure_distance_cm(float tempC) {
    float v = (331.3f + 0.606f * tempC) / 10000.0f; // speed of sound (cm/us)

    // Trigger SR04 pulse
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(3);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // Wait for echo to go HIGH
    int timeout = 30000;
    while (gpio_get_level(ECHO_PIN) == 0 && timeout--) {
        esp_rom_delay_us(1);
    }
    if (timeout <= 0) return -1;

    int64_t start = esp_timer_get_time();

    // Wait for echo to go LOW
    timeout = 30000;
    while (gpio_get_level(ECHO_PIN) == 1 && timeout--) {
        esp_rom_delay_us(1);
    }
    if (timeout <= 0) return -1;

    int64_t end = esp_timer_get_time();

    float pulse = (float)(end - start); // microseconds
    float dist = (v * pulse) / 2.0f;

    return dist;
}

void app_main(void)
{
    gpio_config_t io = {0};

    // TRIG as output
    io.intr_type = GPIO_INTR_DISABLE;
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << TRIG_PIN);
    gpio_config(&io);

    // ECHO as input
    io.intr_type = GPIO_INTR_DISABLE;
    io.mode = GPIO_MODE_INPUT;
    io.pin_bit_mask = (1ULL << ECHO_PIN);
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io);
    
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");

    //TRY TO DO STUFF:
    while(1){
        ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x3517));//TRANSMIT WAKEUP COMMANd
        vTaskDelay(pdMS_TO_TICKS(750));
        ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x5C24 ));//TRANSMIT MEASUREMENT COMMAND(READ RH First, Clock Strething
        
        vTaskDelay(pdMS_TO_TICKS(750));
        uint16_t RH_data;
        uint16_t T_data;
        ESP_ERROR_CHECK(shct3_register_read(dev_handle,&RH_data, &T_data));//RECEIVE THE RH VALUE AND PUT IT IN RH_data;
        //ESP_LOGI(TAG, "read is done");								   
        vTaskDelay(pdMS_TO_TICKS(750));
        ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0xB098));//TRANSMIT SLEEP COMMAND
        
        float  tempC = -45 + (175 * T_data)/(1 << 16);//in C
        float dist  = measure_distance_cm(tempC);

        if (dist < 0) {
            printf("Distance ERROR at %.1fC\n", tempC);
        } else {
            printf("Distance: %.2f cm at %.1fC\n", dist, tempC);
        }

        vTaskDelay(pdMS_TO_TICKS(500));


    
    
    
    }

    
}
