#include "DFRobot_RGBLCD1602.h"
#include <stdio.h>

static const uint8_t color_define[4][3] = {
    {255, 255, 255}, // WHITE
    {255, 0, 0},     // RED
    {0, 255, 0},     // GREEN
    {0, 0, 255}      // BLUE
};

DFRobot_RGBLCD1602::DFRobot_RGBLCD1602(uint8_t lcdAddr, uint8_t RGBAddr)
    : _lcdAddr(lcdAddr), _RGBAddr(RGBAddr) {}

void DFRobot_RGBLCD1602::init(i2c_port_t i2c_port, gpio_num_t sda, gpio_num_t scl) {
    _i2c_port = i2c_port;

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(i2c_port, &conf);
    i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);

    //sendCommand(LCD_FUNCTIONSET | 0x00);
    sendCommand(LCD_FUNCTIONSET | 0x08);
    sendCommand(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
    clear();
    sendCommand(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
    //setColor(WHITE);

    setReg(0x00,0x00);
    setReg(0x01,0x00);
    setReg(0x08,0xFF);
    setRGB(255,255,255);
}

void DFRobot_RGBLCD1602::clear() {
    sendCommand(LCD_CLEARDISPLAY);
    usleep(2000);
}

void DFRobot_RGBLCD1602::home() {
    sendCommand(LCD_RETURNHOME);
    usleep(2000);
}

void DFRobot_RGBLCD1602::setCursor(uint8_t col, uint8_t row) {
    uint8_t addr = (row == 0 ? 0x80 : 0xC0) + col;
    sendCommand(addr);
}

void DFRobot_RGBLCD1602::writeChar(char c) {
    sendData(c);
}

void DFRobot_RGBLCD1602::printstr(const char* str) {
    while (*str) {
        writeChar(*str++);
    }
}

void DFRobot_RGBLCD1602::setColor(uint8_t color) {
    if(color > 3) return;
    setRGB(color_define[color][0], color_define[color][1], color_define[color][2]);
}

void DFRobot_RGBLCD1602::setRGB(uint8_t r, uint8_t g, uint8_t b) {
    setReg(0x04, r); // RED
    setReg(0x03, g); // GREEN
    setReg(0x02, b); // BLUE
}

void DFRobot_RGBLCD1602::sendCommand(uint8_t cmd) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (_lcdAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x80, true);
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(_i2c_port, handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(handle);
}

void DFRobot_RGBLCD1602::sendData(uint8_t data) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (_lcdAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x40, true);
    i2c_master_write_byte(handle, data, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(_i2c_port, handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(handle);
}

void DFRobot_RGBLCD1602::setReg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (_RGBAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, reg, true);
    i2c_master_write_byte(handle, value, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(_i2c_port, handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(handle);
}
