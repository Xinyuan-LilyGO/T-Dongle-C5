#include "st7735.h"

Adafruit_ST7735::Adafruit_ST7735(int8_t cs, int8_t dc, int8_t rst, int8_t sck, int8_t mosi)
    : _cs(cs), _dc(dc), _rst(rst), _sck(sck), _mosi(mosi), _colmod(0x05) {}

void Adafruit_ST7735::begin()
{
    pinMode(_rst, OUTPUT);
    pinMode(_dc, OUTPUT);
    pinMode(_cs, OUTPUT);

    // Hardware reset
    digitalWrite(_rst, HIGH);
    delay(100);
    digitalWrite(_rst, LOW);
    delay(100);
    digitalWrite(_rst, HIGH);
    delay(120);

    if (_sck != -1 || _mosi != -1)
    {
        SPI.begin(_sck, -1, _mosi, -1); // 只配置SCK和MOSI
    }
    else
    {
        SPI.begin(); // 保持默认引脚
    }
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);

    sendInitCommands();
}

void Adafruit_ST7735::setRotation(uint8_t m)
{
    _rotation = m % 4;
    if (_rotation == 1 || _rotation == 3)
    {
        _width = 160;
        _height = 80;
    }
    else
    {
        _width = 80;
        _height = 160;
    }

    static const uint8_t rotation_config[4] = {
        0x00, // 竖屏模式0 (默认)
        0x60, // 横屏模式1: MX+MV
        0xC0, // 竖屏模式2: MX+MY
        0xA0  // 横屏模式3: MY+MV
    };

    if (m > 3)
        m = 3; // 限制模式范围

    _madctl = (rotation_config[m] & 0xF7) | 0x08;
    writeCommand(ST7735_MADCTL);
    spiWrite(_madctl);
}

void Adafruit_ST7735::startWrite()
{
    digitalWrite(_cs, LOW);
    SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
}

void Adafruit_ST7735::writePixels(uint16_t *pixels, uint32_t len)
{
    digitalWrite(_dc, HIGH);
    SPI.transferBytes((uint8_t *)pixels, nullptr, len * 2);
}

void Adafruit_ST7735::endWrite()
{
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
}

void Adafruit_ST7735::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    setAddrWindow(x, y, x + 1, y + 1);
    digitalWrite(_dc, HIGH);
    SPI.transfer16(color);
    digitalWrite(_cs, HIGH);
}

void Adafruit_ST7735::fillScreen(uint16_t color)
{
    setAddrWindow(0, 0, _width - 1, _height - 1);
    digitalWrite(_dc, HIGH);
    digitalWrite(_cs, LOW);
    for (uint32_t i = 0; i < _width * _height; i++)
    {
        SPI.transfer16(color);
    }
    digitalWrite(_cs, HIGH);
}

void Adafruit_ST7735::sendInitCommands()
{
    const uint8_t initCmds[] = {
        ST7735_SWRESET, DELAY, 150,
        ST7735_SLPOUT, DELAY, 255,
        ST7735_FRMCTR1, 3, 0x01, 0x2C, 0x2D,
        ST7735_FRMCTR2, 3, 0x01, 0x2C, 0x2D,
        ST7735_FRMCTR3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,
        ST7735_INVCTR, 1, 0x07,
        ST7735_PWCTR1, 3, 0xA2, 0x02, 0x84,
        ST7735_PWCTR2, 1, 0xC5,
        ST7735_PWCTR3, 2, 0x0A, 0x00,
        ST7735_PWCTR4, 2, 0x8A, 0x2A,
        ST7735_PWCTR5, 2, 0x8A, 0xEE,
        ST7735_VMCTR1, 1, 0x0E,
        ST7735_INVON, 0,
        // ST7735_INVOFF ,1,0
        ST7735_COLMOD, 1, _colmod,
        ST7735_MADCTL, 1, _madctl,
        ST7735_GMCTRP1, 16, 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
        ST7735_GMCTRN1, 16, 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
        ST7735_NORON, DELAY, 10,
        ST7735_DISPON, DELAY, 100,
        0x00};

    uint8_t *cmd = (uint8_t *)initCmds;
    while (cmd[0] != 0x00)
    {
        writeCommand(cmd[0]);
        if (cmd[1] == DELAY)
        {
            delay(cmd[2]);
            cmd += 3;
        }
        else
        {
            for (uint8_t i = 0; i < cmd[1]; i++)
                spiWrite(cmd[2 + i]);
            cmd += (2 + cmd[1]);
        }
    }
}

void Adafruit_ST7735::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if (_rotation == 1 || _rotation == 3)
    {
        x0 += y_gap; // 1
        x1 += y_gap;
        y0 += x_gap; // 26
        y1 += x_gap;
    }
    else
    {
        x0 += x_gap;
        x1 += x_gap;
        y0 += y_gap;
        y1 += y_gap;
    }

    writeCommand(ST7735_CASET);
    spiWrite(x0 >> 8);
    spiWrite(x0 & 0xFF);
    spiWrite(x1 >> 8);
    spiWrite(x1 & 0xFF);
    writeCommand(ST7735_RASET);
    spiWrite(y0 >> 8);
    spiWrite(y0 & 0xFF);
    spiWrite(y1 >> 8);
    spiWrite(y1 & 0xFF);
    writeCommand(ST7735_RAMWR);

}

void Adafruit_ST7735::writeCommand(uint8_t c)
{
    digitalWrite(_dc, LOW);
    digitalWrite(_cs, LOW);
    SPI.transfer(c);
    digitalWrite(_cs, HIGH);
}

void Adafruit_ST7735::spiWrite(uint8_t d)
{
    digitalWrite(_dc, HIGH);
    digitalWrite(_cs, LOW);
    SPI.transfer(d);
    digitalWrite(_cs, HIGH);
}
