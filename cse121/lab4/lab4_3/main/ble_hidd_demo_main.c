/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "driver/gpio.h"
#include "hid_dev.h"

#include "sdkconfig.h"
#include "driver/i2c_master.h"
#include <unistd.h>


#define HID_DEMO_TAG "HID_DEMO"


//from i2c example.
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
#define ICM_42670_ADDR	0x68 //slave addr of the accelerometer
//END OF MY DEFINITION


static esp_err_t shct3_register_write_byte(i2c_master_dev_handle_t dev_handle, uint16_t data)//data is 16 bits
{
	uint8_t write_buf[2] = {((data&0xff00)>>8),(data&0xff)};//concatenate data
	//ESP_LOGI(TAG, "write bruf done");
	esp_err_t err;
	err =  i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf),I2C_MASTER_TIMEOUT_MS);
	//ESP_LOGI(TAG, "master transmit done");
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


static i2c_master_dev_handle_t dev_handle; //GLOBAL DEC of devhandle






static uint16_t hid_conn_id = 0;//1 for mouse? it was 0 initially
static bool sec_conn = false;
static bool send_volum_up = false;
#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

#define HIDD_DEVICE_NAME            "Kailan"
static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                //esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
                esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
                esp_ble_gap_config_adv_data(&hidd_adv_data);

            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
	     break;
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            hid_conn_id = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            sec_conn = false;
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->vendor_write.data, param->vendor_write.length);
            break;
        }
        case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->led_write.data, param->led_write.length);
            break;
        }
        default:
            break;
    }
    return;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
     case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
             ESP_LOGD(HID_DEMO_TAG, "%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
	 break;
     case ESP_GAP_BLE_AUTH_CMPL_EVT:
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(HID_DEMO_TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(HID_DEMO_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(HID_DEMO_TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (param->ble_security.auth_cmpl.success) {
            sec_conn = true;
            ESP_LOGI(HID_DEMO_TAG, "secure connection established.");
        } else {
            ESP_LOGE(HID_DEMO_TAG, "pairing failed, reason = 0x%x",
                     param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    default:
        break;
    }
}

void hid_demo_task(void *pvParameters)
{
    ESP_LOGI(HID_DEMO_TAG, "entered hid_demo_task");
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    int last_button_state = 1;
    while(1) {


        int16_t accelx,accely,accelz;
        uint8_t read_buf[6];
        uint8_t writeBuf[1] = {0x0B}; 
        ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle,writeBuf, sizeof(writeBuf),read_buf,6,I2C_MASTER_TIMEOUT_MS));

    
        accelx  = (read_buf[0]  << 8) | (read_buf[1]);
        accely  = (read_buf[2] << 8) | (read_buf[3]);
        accelz = (read_buf[4] << 8) | (read_buf[5]);



        int16_t accel_x = accelx/2048;//use these for checking movement
        int16_t accel_y = accely/2048;

        int button_state = gpio_get_level(GPIO_NUM_9);//get button state
        //int16_t accel_z = accelz/2048;
    
        
         //char directionString[32];
         //directionString[0] = '\0';

         //if(accelx/2048 >= 4){
         //strcat(directionString, "RIGHT ");
         //}  else if(accelx/2048 <= -4){
         //strcat(directionString, "LEFT ");
         //}


         //if(accely/2048 >= 4){
         //    strcat(directionString, "UP ");
         //} else if(accely/2048 <= -4){
         //    strcat(directionString, "DOWN ");
         //}

    
         //ESP_LOGI(TAG, "%s",directionString);
        

        // sleep(1);
	//vTaskDelay(1000/ portTICK_PERIOD_MS);

        int16_t delta_x;
        int16_t delta_y;

        if(accel_x >= 3 || accel_x <= -3){
            delta_x = accel_x;//scales based on accel_x's value only if right or left
        } else{
            delta_x = 0;
        }

        if(accel_y >= 3 || accel_y <= -3){
            delta_y = (-1)*accel_y;//scales based on accel_y's value only if up or down(flipping it so up is down and vise versa)
        } else{
            delta_y = 0;
        }

        //vTaskDelay(2000 / portTICK_PERIOD_MS);
        if (sec_conn) {//if connected via bluetooth
            //ESP_LOGI(HID_DEMO_TAG, "Send the mouse input");
            send_volum_up = true;
            //vTaskDelay(2000 / portTICK_PERIOD_MS);
            if (send_volum_up) {
		    send_volum_up = false;
	       	esp_hidd_send_mouse_value(hid_conn_id,0,delta_x*4,delta_y*4);//HID_MOUSE_LEFT is irrellevant rn, i think its used for click later on.//scaled up by 4
                //vTaskDelay(2000 / portTICK_PERIOD_MS);
            }

            //button press
            if (last_button_state == 1 && button_state == 0) {
                ESP_LOGI(HID_DEMO_TAG, "Button pressed -> Mouse Click");
                esp_hidd_send_mouse_value(hid_conn_id, 1, 0, 0); // press left button
                vTaskDelay(50 / portTICK_PERIOD_MS); // short debounce delay
                esp_hidd_send_mouse_value(hid_conn_id, 0, 0, 0); // release
            }


	   // vTaskDelay(2000/ portTICK_PERIOD_MS);
        }
    last_button_state = button_state;
	vTaskDelay(150/ portTICK_PERIOD_MS);
	//ESP_LOGI(HID_DEMO_TAG, "end of while loop");
    }
}


void app_main(void)
{
    esp_err_t ret;
    i2c_master_bus_handle_t bus_handle;
   //i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");


    ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x1F0F));//attempt to access pwr management,(1F) in order to power o Gyro and Acc (0F)
    ESP_LOGI(TAG, "They should be powered on");
    ESP_ERROR_CHECK(shct3_register_write_byte(dev_handle, 0x2165));//access the acce config stuff (0x21, and 0x65 for the settings)

    //GPIO STUFF
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << 9,     // GPIO9
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_LOGI(TAG, "Button (GPIO9) configured as input with pull-up");
    //END OF GPIO STUFF

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed", __func__);
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed", __func__);
        return;
    }

    esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
        return;
    }

    if((ret = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
    }

    ///register the callback function to the gap module
    esp_ble_gap_register_callback(gap_event_handler);
    esp_hidd_register_callbacks(hidd_event_callback);

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    ESP_LOGE(HID_DEMO_TAG, "xtaskcreatecomingup");
    xTaskCreate(&hid_demo_task, "hid_task", 2048, NULL, 5, NULL);
}
