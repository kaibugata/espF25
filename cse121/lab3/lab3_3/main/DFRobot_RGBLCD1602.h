#pragma once
#include <cstdint>
#include "driver/i2c.h"
#include <cstring>
#include <unistd.h>

#define LCD_ADDRESS     (0x7c >> 1)

#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME   0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_FUNCTIONSET 0x20
#define LCD_SETDDRAMADDR 0x80

#define LCD_ENTRYLEFT    0x02
#define LCD_ENTRYSHIFTDECREMENT 0x00

#define LCD_DISPLAYON    0x04
#define LCD_CURSOROFF    0x00
#define LCD_BLINKOFF     0x00

#define WHITE 0
#define RED   1
#define GREEN 2
#define BLUE  3

class DFRobot_RGBLCD1602 {
public:
    DFRobot_RGBLCD1602(uint8_t lcdAddr = LCD_ADDRESS, uint8_t RGBAddr = 0x62);

    void init(i2c_port_t i2c_port, gpio_num_t sda, gpio_num_t scl);
    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void writeChar(char c);
    void printstr(const char* str);
    void setColor(uint8_t color);
    void setRGB(uint8_t r, uint8_t g, uint8_t b);

private:
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void setReg(uint8_t reg, uint8_t value);

    uint8_t _lcdAddr;
    uint8_t _RGBAddr;
    i2c_port_t _i2c_port;
};
