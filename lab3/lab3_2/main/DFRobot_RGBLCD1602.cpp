/*!
 * @file DFRobot_RGBLCD1602.cpp
 * @brief DFRobot_RGBLCD1602 class infrastructure, the implementation of basic methods
 * @copyright	Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence     The MIT License (MIT)
 * @maintainer [yangfeng](feng.yang@dfrobot.com)
 * @version  V1.0
 * @date  2021-09-24
 * @url https://github.com/DFRobot/DFRobot_RGBLCD1602
 *
 *  All functions are the same as the original file
 *  unless otherwise marked with:
 *  //
 *  // *ESP32C3 CHANGE*
 *  //
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "DFRobot_RGBLCD1602.h"

//
// *ESP32C3 CHANGE*
//
// create commands to allow delay commands to still function 
static inline void delayMicroseconds(uint32_t us){ esp_rom_delay_us(us); }
static inline void delay(uint32_t ms)
{
    if (ms < 10) esp_rom_delay_us(ms * 1000);           // freertos ticks are 10ms by default
    else vTaskDelay(pdMS_TO_TICKS(ms));
}

const uint8_t color_define[4][3] = 
{
    {255, 255, 255},            // white
    {255, 0, 0},                // red
    {0, 255, 0},                // green
    {0, 0, 255},                // blue
};

/*******************************public*******************************/

//
// *ESP32C3 CHANGE*
//
// new constructor 
DFRobot_RGBLCD1602::DFRobot_RGBLCD1602(uint8_t RGBAddr, uint8_t lcdCols,uint8_t lcdRows,
                                       i2c_master_bus_handle_t bus, uint8_t lcdAddr)
{
  _lcdAddr = lcdAddr;
  _RGBAddr = RGBAddr;
  _cols = lcdCols;
  _rows = lcdRows;
  _bus = bus;                   // replace pWire with bus 
  _lcdDev = nullptr;            // handle lcdDev
  _rgbDev = nullptr;            // handle rgbDev
}

//
// *ESP32C3 CHANGE*
//
// initialization 
void DFRobot_RGBLCD1602::init()
{
  // configure device with proper address length and clk speed 
  i2c_device_config_t cfg = {};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.scl_speed_hz = 100000;
  cfg.device_address = _lcdAddr;            // we will add the LCD first 

  // LCD and RGB are separate devices, must be added to bus separately 
  i2c_master_bus_add_device(_bus, &cfg, &_lcdDev);
  cfg.device_address = _RGBAddr;
  i2c_master_bus_add_device(_bus, &cfg, &_rgbDev); 

  if(_RGBAddr == (0x60)){
    REG_RED   =      0x04;
    REG_GREEN =      0x03;
    REG_BLUE  =      0x02;
    REG_ONLY  =      0x02 ;
  } else if(_RGBAddr == (0x60>>1)){
    REG_RED      =   0x06 ;       // pwm2
    REG_GREEN    =   0x07 ;       // pwm1
    REG_BLUE     =   0x08 ;       // pwm0
    REG_ONLY     =   0x08 ;
  } else if(_RGBAddr == (0x6B)){
    REG_RED      =   0x06 ;       // pwm2
    REG_GREEN    =   0x05 ;       // pwm1
    REG_BLUE     =   0x04 ;       // pwm0
    REG_ONLY     =   0x04 ; 
  } else if(_RGBAddr == (0x2D)){
    REG_RED      =   0x01 ;       // pwm2
    REG_GREEN    =   0x02 ;       // pwm1
    REG_BLUE     =   0x03 ;       // pwm0
    REG_ONLY     =   0x01 ; 
  }
  _showFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
  begin(_rows);
}

void DFRobot_RGBLCD1602::clear()
{
    command(LCD_CLEARDISPLAY);        // clear display, set cursor position to zero
    delayMicroseconds(2000);          // this command takes a long time!
}

void DFRobot_RGBLCD1602::home()
{
    command(LCD_RETURNHOME);        // set cursor position to zero
    delayMicroseconds(2000);        // this command takes a long time!
}

void DFRobot_RGBLCD1602::noDisplay()
{
    _showControl &= ~LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_RGBLCD1602::display() {
    _showControl |= LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_RGBLCD1602::stopBlink()
{
    _showControl &= ~LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _showControl);
}
void DFRobot_RGBLCD1602::blink()
{
    _showControl |= LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_RGBLCD1602::noCursor()
{
    _showControl &= ~LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_RGBLCD1602::cursor() {
    _showControl |= LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_RGBLCD1602::scrollDisplayLeft(void)
{
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void DFRobot_RGBLCD1602::scrollDisplayRight(void)
{
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void DFRobot_RGBLCD1602::leftToRight(void)
{
    _showMode |= LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_RGBLCD1602::rightToLeft(void)
{
    _showMode &= ~LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_RGBLCD1602::noAutoscroll(void)
{
    _showMode &= ~LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_RGBLCD1602::autoscroll(void)
{
    _showMode |= LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_RGBLCD1602::customSymbol(uint8_t location, uint8_t charmap[])
{

    location &= 0x7; // we only have 8 locations 0-7
    command(LCD_SETCGRAMADDR | (location << 3));
    
    
    uint8_t data[9];
    data[0] = 0x40;
    for(int i=0; i<8; i++)
    {
        data[i+1] = charmap[i];
    }
    send(data, 9);
}

void DFRobot_RGBLCD1602::setCursor(uint8_t col, uint8_t row)
{

    col = (row == 0 ? col|0x80 : col|0xc0);
    uint8_t data[3] = {0x80, col};

    send(data, 2);

}

void DFRobot_RGBLCD1602::setRGB(uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t temp_r,temp_g,temp_b;
  if(_RGBAddr == 0x60>>1){
    temp_r = (uint16_t)r*192/255;
    temp_g = (uint16_t)g*192/255;
    temp_b = (uint16_t)b*192/255;
    setReg(REG_RED, temp_r);
    setReg(REG_GREEN, temp_g);
    setReg(REG_BLUE, temp_b);
  } else{
    setReg(REG_RED, r);
    setReg(REG_GREEN, g);
    setReg(REG_BLUE, b);
    if(_RGBAddr == 0x6B){
      setReg(0x07, 0xFF);
    }
  }

}

void DFRobot_RGBLCD1602::setColor(uint8_t color)
{
    if(color > 3)return ;
    setRGB(color_define[color][0], color_define[color][1], color_define[color][2]);
}

//
// *ESP32C3 CHANGE*
//
// overloaded write command takes a string and uses other write function to display 
size_t DFRobot_RGBLCD1602::write(const char *str)
{
    size_t n = 0; 
    while (*str)                    // iterate through string 
    { 
        write((uint8_t) *str++);    // convert each char to a byte, use other write function
        n++;
    } 
    return n; 
}

size_t DFRobot_RGBLCD1602::write(uint8_t value)
{
    uint8_t data[3] = {0x40, value};
    send(data, 2);
    return 1;
}

void DFRobot_RGBLCD1602::command(uint8_t value)
{
    uint8_t data[3] = {0x80, value};
    send(data, 2);
}



void DFRobot_RGBLCD1602::setBacklight(bool mode)
{
	if(mode){
		setColorWhite();		// turn backlight on
	}else{
		closeBacklight();		// turn backlight off
	}
}

/*******************************private*******************************/
void DFRobot_RGBLCD1602::begin( uint8_t rows, uint8_t charSize) 
{
    if (rows > 1) {
        _showFunction |= LCD_2LINE;
    }
    _numLines = rows;
    _currLine = 0;
    ///< for some 1 line displays you can select a 10 pixel high font
    if ((charSize != 0) && (rows == 1)) {
        _showFunction |= LCD_5x10DOTS;
    }

    ///< SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
    ///< according to datasheet, we need at least 40ms after power rises above 2.7V
    ///< before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
    delay(50);

    ///< this is according to the hitachi HD44780 datasheet
    ///< page 45 figure 23

    ///< Send function set command sequence
    command(LCD_FUNCTIONSET | _showFunction);
    delay(5);  // wait more than 4.1ms
	
	///< second try
    command(LCD_FUNCTIONSET | _showFunction);
    delay(5);

    ///< third go
    command(LCD_FUNCTIONSET | _showFunction);

    ///< turn the display on with no cursor or blinking default
    _showControl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    display();

    ///< clear it off
    clear();

    ///< Initialize to default text direction (for romance languages)
    _showMode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    ///< set the entry mode
    command(LCD_ENTRYMODESET | _showMode);
    
    if(_RGBAddr == (0xc0>>1)){
      ///< backlight init
      setReg(REG_MODE1, 0);
      ///< set LEDs controllable by both PWM and GRPPWM registers
      setReg(REG_OUTPUT, 0xFF);
      ///< set MODE2 values
      ///< 0010 0000 -> 0x20  (DMBLNK to 1, ie blinky mode)
      setReg(REG_MODE2, 0x20);
    }else if(_RGBAddr == (0x60>>1)){
       setReg(0x01, 0x00);
       setReg(0x02, 0xfF);
       setReg(0x04, 0x15);
    }else if(_RGBAddr==0x6B){
        setReg(0x2F, 0x00);
        setReg(0x00, 0x20);
        setReg(0x01, 0x00);
        setReg(0x02, 0x01);
        setReg(0x03, 4);
    }
    setColorWhite();
}

//
// *ESP32C3 CHANGE*
//
// Send fuction modified to send commands to the screen using 
// i2c protocol rather than arduino 
void DFRobot_RGBLCD1602::send(uint8_t *data, uint8_t len)
{
    i2c_master_transmit(_lcdDev, data, len, 100);           // transmit command
    delayMicroseconds(100);                                 // wait before anything else  
}

//
// *ESP32C3 CHANGE*
//
// setreg command is also simplified to use i2c protocol 
void DFRobot_RGBLCD1602::setReg(uint8_t addr, uint8_t data)
{
    uint8_t buf[2] = {addr, data};                          // concatenate address and data bytes to buffer
    i2c_master_transmit(_rgbDev, buf, 2, 100);              // transmit both 
}