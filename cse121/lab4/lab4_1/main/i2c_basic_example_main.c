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
#define ICM_42670_ADDR	0x68 //slave addr of the gyroscope
//END OF MY DEFINITION




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
static esp_err_t shct3_register_read(i2c_master_dev_handle_t dev_handle, uint16_t *accel_x, uint16_t *accel_y, uint16_t *accel_z)
{
	

	uint8_t read_buf[6] = {0}; //16'b accel x, 16'b accel y, 16'd accel z
	//ESP_LOGI(TAG, "read buf init");
	esp_err_t err;
	err =  i2c_master_receive(dev_handle,read_buf ,sizeof(read_buf) ,I2C_MASTER_TIMEOUT_MS);//testing using master_receive instead of master_transmit_receive
	//ESP_LOGI(TAG, "err def");

	*accel_x  = (read_buf[0]  << 8) | (read_buf[1]);
	*accel_y  = (read_buf[2] << 8) | (read_buf[3]);
	*accel_z = (read_buf[4] << 8) | (read_buf[5]);
       	//ESP_LOGI(TAG, "accel data gotten");
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
        .device_address = ICM_42670_ADDR, //modified
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

void app_main(void)
{
    //uint8_t data[2];
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");


    ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x1F0F));//attempt to access pwr management,(1F) in order to power o Gyro and Acc (0F)
    ESP_LOGI(TAG, "They should be powered on");
    ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x2165));//access the acce config stuff (0x21, and 0x65 for the settings)
			  

    //TRY TO DO STUFF:
    while(1){
								   
    int16_t accelx,accely,accelz;
    uint8_t read_buf[6];
    uint8_t writeBuf[1] = {0x0B}; 
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle,writeBuf, sizeof(writeBuf),read_buf,6,I2C_MASTER_TIMEOUT_MS));

 
    accelx  = (read_buf[0]  << 8) | (read_buf[1]);
    accely  = (read_buf[2] << 8) | (read_buf[3]);
    accelz = (read_buf[4] << 8) | (read_buf[5]);
  
    //ESP_LOGI(TAG, "accel_X = %d, accel_Y = %d, accel_Z = %d",accelx/2048,accely/2048,accelz/2048);
    char directionString[32];
    directionString[0] = '\0';

    if(accelx/2048 >= 4){
	strcat(directionString, "RIGHT ");
    }  else if(accelx/2048 <= -4){
	strcat(directionString, "LEFT ");
    }


    if(accely/2048 >= 4){
    	strcat(directionString, "UP ");
    } else if(accely/2048 <= -4){
    	strcat(directionString, "DOWN ");
    }

   
    ESP_LOGI(TAG, "%s",directionString);
    

    sleep(1); 
    
    }

}
