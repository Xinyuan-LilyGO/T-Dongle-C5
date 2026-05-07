#ifndef _ST7735H_
#define _ST7735H_
#pragma once

#include <Arduino.h>
#include <SPI.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ST7735_BLACK 0x0000
#define ST7735_WHITE 0xFFFF
#define ST7735_BLUE  0x001F
    // 添加其他颜色定义...

#define ST7735_GREENTAB160x80 // For 160 x 80 display (BGR, inverted, 26 / 1 offset) 0.96 tft
#define COLSTART 26
#define ROWSTART 1

// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x80

#define TFT_INVOFF 0x20
#define TFT_INVON 0x21

// ST7735 specific commands used in init
#define ST7735_NOP 0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID 0x04
#define ST7735_RDDST 0x09

#define ST7735_SLPIN 0x10
#define ST7735_SLPOUT 0x11
#define ST7735_PTLON 0x12
#define ST7735_NORON 0x13

#define ST7735_INVOFF 0x20
#define ST7735_INVON 0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON 0x29
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_RAMRD 0x2E

#define ST7735_PTLAR 0x30
#define ST7735_VSCRDEF 0x33
#define ST7735_COLMOD 0x3A
#define ST7735_MADCTL 0x36
#define ST7735_VSCRSADD 0x37

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR 0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1 0xC0
#define ST7735_PWCTR2 0xC1
#define ST7735_PWCTR3 0xC2
#define ST7735_PWCTR4 0xC3
#define ST7735_PWCTR5 0xC4
#define ST7735_VMCTR1 0xC5

#define ST7735_RDID1 0xDA
#define ST7735_RDID2 0xDB
#define ST7735_RDID3 0xDC
#define ST7735_RDID4 0xDD

#define ST7735_PWCTR6 0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

#ifdef __cplusplus
}
#endif

class Adafruit_ST7735
{
public:
    Adafruit_ST7735(int8_t cs, int8_t dc, int8_t rst, int8_t sck = -1, int8_t mosi = -1);
    void begin();
    void setRotation(uint8_t m);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void fillScreen(uint16_t color);
    void startWrite();
    void writePixels(uint16_t *pixels, uint32_t len);
    void endWrite();
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

private:
    int8_t _cs, _dc, _rst,_sck, _mosi;
    uint8_t _colmod;
    uint8_t _madctl = 0x00;
    uint16_t _width = 80;
    uint16_t _height = 160;
    uint16_t x_gap = 26;
    uint16_t y_gap = 1;
    uint16_t _rotation = 0;

    void sendInitCommands();
    void writeCommand(uint8_t c);
    void spiWrite(uint8_t d);
    enum
    {
        DELAY = 0x80
    };
};

#endif
