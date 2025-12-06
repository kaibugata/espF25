#include <stdio.h>
#include "DFRobot_RGBLCD1602.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_PORT I2C_NUM_0
#define SDA_PIN 10
#define SCL_PIN 8

DFRobot_RGBLCD1602 lcd(0x7c >> 1, 0x2D);

extern "C" void app_main(void) {
    lcd.init(I2C_PORT, (gpio_num_t)SDA_PIN, (gpio_num_t)SCL_PIN);

    while (true) {
        lcd.clear();

        //lcd.setColor(WHITE);
	lcd.setRGB(255,255,255);
	//lcd.setColorWhite();

        lcd.setCursor(0, 0);
        lcd.printstr("Hello CMPE 121");

        lcd.setCursor(0, 1);
        lcd.printstr("Kailan");

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
