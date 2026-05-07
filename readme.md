<h1 align = "center">🎯 T-Dongle-C5 🌐</h1>

## 🌈Introduction

| board       | MCU         | WIFI   | BLE     | Display                  | storage | Flash | PSRAM | RGB(LED) | Button | USB   |
| ----------- | ----------- | ------ | ------- | ------------------------ | ------- | ----- | ----- | -------- | ------ | ----- |
| T-Dongle-C5 | ESP32-C5HR8 | WIFI 6 | BLE 5.0 | ST7735-80*160(0.96 inch) | SD      | 16MB  | 8MB   | APA102   | Boot   | USB-A |

### Features

## 🛵Directory
```
T-Dongle-C5
├── Examples
|   ├── BLE_Connect : BLE connection demo
|   ├── SD_LED     : SD card and LED demo
|   ├── SPI_LCD     : SPI LCD demo
|   ├── T-Dongle-C5 : T-Dongle-C5 Factory Exampel
|   ├── WiFi_AP     : WiFi AP demo
|   ├── WiFI_STA    : WiFi STA demo
├── Hardware
│   ├── T-Dongle-C5.pdf : T-Dongle-C5 hardware design
├── firmware ：T-Dongle-C5 firmware
├── shcematic ：ESP32C5 datasheet and manual
|   
├── readme.md
```

## 🚩Pinout Diagram
| GPIO    | LCD      | LED    | SD     | UART     | Button | USB    |
| ------- | -------- | ------ | ------ | -------- | ------ | ------ |
| GPIO_2  | LCD_MOSI |        | SD_CMD |          |        |        |
| GPIO_4  |          | LED_CI |        |          |        |        |
| GPIO_5  |          | LED_DI |        |          |        |        |
| GPIO_7  | LCD_MISO |        | SD_D0  |          |        |        |
| GPIO_6  | LCD_SCK  |        | SD_CLK |          |        |        |
| GPIO_10 | LCD_CS   |        |        |          |        |        |
| GPIO_3  | LCD_RS   |        |        |          |        |        |
| GPIO_0  | LCD_BL   |        |        |          |        |        |
| GPIO_1  | LCD_RST  |        |        |          |        |        |
| GPIO_23 |          |        | SD_CS  |          |        |        |
| GPIO_28 |          |        |        |          | BOOT   |        |
| GPIO_11 |          |        |        | UART0_TX |        |        |
| GPIO_12 |          |        |        | UART0_RX |        |        |
| GPIO_13 |          |        |        |          |        | USB_DN |
| GPIO_14 |          |        |        |          |        | USB_DP |

## ⚙️Quick Start
- The IDF installation method is also inconsistent depending on the system, it is recommended to refer to the [official manual](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) for installation

| software   | supported | version          |
| ---------- | --------- | ---------------- |
| ESP-IDF    | ✅         | v5.5 or higher   |
| PlatfromIO | ✅         |                  |
| Arduino    | ✅         | v3.3.0 or higher |

## 🚀FAQ 

