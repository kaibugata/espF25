/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* i2c - Simple Example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU9250 inertial measurement unit.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include <unistd.h>
#include "DFRobot_RGBLCD1602.h"

static const char *TAG = "example";

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define MPU9250_SENSOR_ADDR         0x68        /*!< Address of the MPU9250 sensor */
#define MPU9250_WHO_AM_I_REG_ADDR   0x75        /*!< Register addresses of the "who am I" register */
#define MPU9250_PWR_MGMT_1_REG_ADDR 0x6B        /*!< Register addresses of the power management register */
#define MPU9250_RESET_BIT           7




//MY DEFINITIONS:
#define SHTC3_ADDR	0x70	/*!< Address of the SHTC3 Sensor */
#define SDA_PIN 10
#define SCL_PIN 8
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

extern "C" void app_main(void)
{
    //uint8_t data[2];
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");


    
    /* Read the MPU9250 WHO_AM_I register, on power up the register should have the value 0x71 */
    //ESP_ERROR_CHECK(mpu9250_register_read(dev_handle, MPU9250_WHO_AM_I_REG_ADDR, data, 1));
    //ESP_LOGI(TAG, "WHO_AM_I = %X", data[0]);

    /* Demonstrate writing by resetting the MPU9250 */
    //ESP_ERROR_CHECK(mpu9250_register_write_byte(dev_handle, MPU9250_PWR_MGMT_1_REG_ADDR, 1 << MPU9250_RESET_BIT));

    //ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    //ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    //ESP_LOGI(TAG, "I2C de-initialized successfully");
    

    //LCD STUFF
    DFRobot_RGBLCD1602 lcd(LCD_ADDR, RGB_ADDR);
    lcd.init(I2C_MASTER_NUM, (gpio_num_t)I2C_MASTER_SDA_IO, (gpio_num_t)I2C_MASTER_SCL_IO);
    lcd.setRGB(0,50,255);
    lcd.clear();




    //TRY TO DO STUFF:
    while(1){
    ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x3517));//TRANSMIT WAKEUP COMMANd
								   //

    sleep(1);
    ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x5C24 ));//TRANSMIT MEASUREMENT COMMAND(READ RH First, Clock Strething
     
    sleep(1);
    uint16_t RH_data;
    uint16_t T_data;
    ESP_ERROR_CHECK(shct3_register_read(dev_handle,&RH_data, &T_data));//RECEIVE THE RH VALUE AND PUT IT IN RH_data;
    //ESP_LOGI(TAG, "read is done");								   
    sleep(1);
    ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0xB098));//TRANSMIT SLEEP COMMAND
    //printf("RH_data is : %d, T_data is : %d\n",RH_data, T_data);
    uint16_t  RH = (100 * RH_data)/(1 << 16);
    //printf("RH:%d\n", RH);
    uint16_t  T = -45 + (175 * T_data)/(1 << 16);//in C
    //printf("T:%d",T);
    //
    char line1[17],line2[17];
    snprintf(line1,sizeof(line1), "Temp: %dC", T);
    snprintf(line2,sizeof(line2), "Humidity: %d%", RH);
    lcd.setCursor(0,0);
    lcd.printstr(line1);
    lcd.setCursour(0,1);
    lcd.printstr(line2);
    //uint16_t  TinF = (T * (9/5)) + 32;//in F
    //printf("Temperature is %dC (or %dF) with a %d humidity\n",T,TinF,RH);
    sleep(1);
    }

    


}
