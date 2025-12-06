#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "DFRobot_RGBLCD1602.h"

static const char *TAG = "SHTC3_LCD";

// ====================== I2C CONFIG ======================
#define I2C_MASTER_SCL_IO           (gpio_num_t)8
#define I2C_MASTER_SDA_IO           (gpio_num_t)10
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY
#define I2C_MASTER_TIMEOUT_MS       1000

// ====================== DEVICE ADDRESSES ======================
#define SHTC3_ADDR 0x70
#define LCD_ADDR   0x3E
#define RGB_ADDR   0x2D

// ====================== SENSOR FUNCTIONS ======================

static esp_err_t shtc3_register_write_word(uint16_t data)
{
    uint8_t write_buf[2] = { static_cast<uint8_t>((data >> 8) & 0xFF),
                             static_cast<uint8_t>(data & 0xFF) };
    return i2c_master_write_to_device(I2C_MASTER_NUM, SHTC3_ADDR,write_buf,2, I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t shtc3_register_read( uint16_t *humidity, uint16_t *temp)
{
    uint8_t read_buf[6] = {0};
    esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM, SHTC3_ADDR, read_buf,6, I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    *humidity = (read_buf[0] << 8) | read_buf[1];
    *temp = (read_buf[3] << 8) | read_buf[4];
    return ESP_OK;
}

//static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
//{
//    i2c_master_bus_config_t bus_config = {
//        .i2c_port = I2C_MASTER_NUM,
//        .sda_io_num = I2C_MASTER_SDA_IO,
//        .scl_io_num = I2C_MASTER_SCL_IO,
//        .clk_source = I2C_CLK_SRC_DEFAULT,
//        .glitch_ignore_cnt = 7,
//        .flags = { .enable_internal_pullup = true }
//    };
//    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
//
//    i2c_device_config_t dev_config = {
//       .dev_addr_length = I2C_ADDR_BIT_LEN_7,
//        .device_address = SHTC3_ADDR,
//        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
//    };
//    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
//}

// ====================== MAIN APPLICATION ======================

extern "C" void app_main(void)
{
   // i2c_master_bus_handle_t bus_handle;
    //i2c_master_dev_handle_t dev_handle;

    // Initialize I2C bus
    //i2c_master_init(&bus_handle, &dev_handle);
    //
    //
    //

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode , 0,0,0));
    //ESP_LOGI(TAG, "I2C initialized successfully");

    // Initialize LCD
    DFRobot_RGBLCD1602 lcd(LCD_ADDR, RGB_ADDR);
    lcd.init(I2C_MASTER_NUM, (gpio_num_t)I2C_MASTER_SDA_IO, (gpio_num_t)I2C_MASTER_SCL_IO);
    lcd.setRGB(0, 80, 255); // Light blue
    lcd.clear();
    lcd.printstr("SHTC3 Sensor");
    lcd.setCursor(0, 1);
    lcd.printstr("Starting...");
    sleep(2);
    lcd.clear();

    while (true) {
        // Wake up sensor
        ESP_ERROR_CHECK(shtc3_register_write_word(0x3517));
        vTaskDelay(pdMS_TO_TICKS(20));

        // Trigger measurement
        ESP_ERROR_CHECK(shtc3_register_write_word(0x5C24));
        vTaskDelay(pdMS_TO_TICKS(50));

        // Read measurement
        uint16_t RH_raw = 0, T_raw = 0;
        if (shtc3_register_read(&RH_raw, &T_raw) == ESP_OK) {
            // Sleep sensor
            shtc3_register_write_word(0xB098);

            // Convert to real values
            float RH = (100.0f * RH_raw) / 65536.0f;
            float T = -45.0f + (175.0f * T_raw) / 65536.0f;

            // Optional: Fahrenheit
            float T_F = T * 9.0f / 5.0f + 32.0f;

            // Print to log
            ESP_LOGI(TAG, "Temp: %.2f C (%.2f F), Humidity: %.2f %%", T, T_F, RH);

            // Print to LCD
            lcd.clear();
            char line1[17];
            char line2[17];
            snprintf(line1, sizeof(line1), "Temp: %.1fC", T);
            snprintf(line2, sizeof(line2), "Hum: %.1f%%", RH);
            lcd.setCursor(0, 0);
            lcd.printstr(line1);
            lcd.setCursor(0, 1);
            lcd.printstr(line2);

            // Optional: change LCD color by temperature
            if (T < 20) lcd.setRGB(0, 0, 255);        // Blue (cold)
            else if (T < 28) lcd.setRGB(0, 255, 0);   // Green (comfortable)
            else lcd.setRGB(255, 0, 0);               // Red (hot)
        } else {
            ESP_LOGW(TAG, "Failed to read from SHTC3");
        }

        vTaskDelay(pdMS_TO_TICKS(3000)); // Update every 3 seconds
    }
}
