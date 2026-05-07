#pragma once

#define BOARD_NAME "T-Dongle-C5"

#define SPI_MOSI 2
#define SPI_MISO 7
#define SPI_SCK  6

#define TFT_HOR_RES 80
#define TFT_VER_RES 160

#define PIN_LCD_MOSI  SPI_MOSI
#define PIN_LCD_SCK   SPI_SCK
#define PIN_LCD_BL    0
#define PIN_LCD_RST   1
#define PIN_LCD_DC    3
#define PIN_LCD_CS    10

#define LED_CI_PIN 4  
#define LED_DI_PIN 5   

#define SD_CMD_PIN SPI_MOSI
#define SD_DAT0_PIN SPI_MISO
#define SD_CLK_PIN SPI_SCK
#define SD_CS_PIN 23

#define UART0_TX_PIN 11
#define UART0_RX_PIN 12