#include <Arduino.h>
#include "pin_config.h"
#include "SPI.h"
#include <lvgl.h>
#include "st7735.h"
#include "FS.h"
#include "SD.h"
#include <FastLED.h> // https://github.com/FastLED/FastLED

#define APA102_LED_NUMBERS 1

CRGB leds;
bool SD_Mount = false;
Adafruit_ST7735 tft = Adafruit_ST7735(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_SCK, PIN_LCD_MOSI);
// SPI配置
SPIClass *hspi = nullptr; 
// LVGL显示缓冲
#define DISP_BUF_SIZE (240 * 40) // 双缓冲每帧40行
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lvgl_init();
// void set_apa102_config(uint8_t brightness, uint32_t color);
void send_apa102_data(uint8_t brightness, uint8_t red, uint8_t green, uint8_t blue);
void testFileIO(fs::FS &fs, const char *path);
void createDir(fs::FS &fs, const char *path);
void removeDir(fs::FS &fs, const char *path);
void readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void appendFile(fs::FS &fs, const char *path, const char *message);
void renameFile(fs::FS &fs, const char *path1, const char *path2);
void deleteFile(fs::FS &fs, const char *path);
void testFileIO(fs::FS &fs, const char *path);

static const uint32_t colors[] = {
    0x0000FF, // 蓝色
    0xFF0000, // 红色
    0x00FF00, // 绿色
    0xFFFF00, // 黄色
    0xFF00FF, // 品红
    0x00FFFF  // 青色
};

void led_task(void *param) {
  while (1) {
    static uint8_t hue = 0;
    leds = CHSV(hue++, 0XFF, 100);
    FastLED.show();
    delay(50);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Hello ESP32C5!");

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(0x001f); // blue background

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, 0);

  FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
  SPI.begin(SD_CLK_PIN, SD_DAT0_PIN, SD_CMD_PIN, SD_CS_PIN);
  // set_apa102_config(31, 0x000000);
  xTaskCreatePinnedToCore(led_task, "led_task", 1024, NULL, 1, NULL, 0);

  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("Card Mount Failed");
    SD_Mount = false;
  }
  else
  {
    SD_Mount = true;
  }

  if (SD_Mount)
  {
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
      Serial.println("No SD card attached");
      return;
    }
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
      Serial.println("MMC");
    }
    else if (cardType == CARD_SD)
    {
      Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
      Serial.println("SDHC");
    }
    else
    {
      Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    listDir(SD, "/", 0);
    createDir(SD, "/mydir");
    listDir(SD, "/", 0);
    removeDir(SD, "/mydir");
    listDir(SD, "/", 2);
    writeFile(SD, "/hello.txt", "Hello ");
    appendFile(SD, "/hello.txt", "World!\n");
    readFile(SD, "/hello.txt");
    deleteFile(SD, "/foo.txt");
    renameFile(SD, "/hello.txt", "/foo.txt");
    readFile(SD, "/foo.txt");
    testFileIO(SD, "/test.txt");
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
  }

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);
  lv_init();
  lvgl_init();

  /* 创建一个简单的UI界面作为测试 */
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0000ff), 0);
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "ESP32-C5 !");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  Serial.println("LVGL and ST7735 initialization complete.");
}

void loop()
{
  static uint32_t last_time = 0;
  static uint8_t color_index = 0;
  if (millis() - last_time > 2000)
  {
    last_time = millis();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(colors[color_index]), 0);
    // color_index = (color_index + 1) % (sizeof(colors) / sizeof(colors[0]));
    // set_apa102_config(31, colors[color_index]);
  }
  lv_task_handler();
  delay(5);
}

void lvgl_init()
{
  // 显示缓冲区初始化
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);
  // 显示驱动注册
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.draw_buf = &draw_buf;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.hor_res = 160;
  disp_drv.ver_res = 80;
  lv_disp_drv_register(&disp_drv);
}

// LVGL显示驱动回调函数：将LVGL的图形缓冲区数据刷新到屏幕上
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  Serial.printf("x1:%d,x2:%d,y1:%d,y2:%d\n", area->x1, area->y1, area->x2, area->y2);
  tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);

  // 批量传输像素数据
  tft.startWrite();
  tft.writePixels((uint16_t *)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp_drv);
}

// void set_apa102_config(uint8_t brightness, uint32_t color)
// {
//   uint8_t red = (color >> 16) & 0xFF;
//   uint8_t green = (color >> 8) & 0xFF;
//   uint8_t blue = color & 0xFF;

//   send_apa102_data(brightness, red, green, blue);
// }

// void send_apa102_data(uint8_t brightness, uint8_t red, uint8_t green, uint8_t blue)
// {
//   size_t buffer_size = (APA102_LED_NUMBERS * 4) + 8; // 起始帧 + LED 数据帧 + 结束帧
//   uint8_t buffer[buffer_size];

//   // 起始帧（4 字节，全为 0）
//   memset(buffer, 0, 4);

//   // 填充 LED 数据帧
//   for (int i = 0; i < APA102_LED_NUMBERS; i++)
//   {
//     buffer[4 + (i * 4)] = 0b11100000 | (brightness & 0x1F); // 亮度
//     buffer[4 + (i * 4) + 1] = blue;                         // 蓝色
//     buffer[4 + (i * 4) + 2] = green;                        // 绿色
//     buffer[4 + (i * 4) + 3] = red;                          // 红色
//   }

//   // 结束帧（至少 LED 数量 / 2 字节，全为 0）
//   memset(buffer + 4 + (APA102_LED_NUMBERS * 4), 0, 4);
//   // for (int i = 0; i < buffer_size; i++) {
//   //   Serial.printf("buffer[%d]:%d\n", i, buffer[i]);
//   // }

//   SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
//   SPI.transfer(buffer, buffer_size);
//   SPI.endTransaction();
// }

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Dir created");
  }
  else
  {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path)
{
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path))
  {
    Serial.println("Dir removed");
  }
  else
  {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2))
  {
    Serial.println("File renamed");
  }
  else
  {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path))
  {
    Serial.println("File deleted");
  }
  else
  {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char *path)
{
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file)
  {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len)
    {
      size_t toRead = len;
      if (toRead > 512)
      {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %lu ms\n", flen, end);
    file.close();
  }
  else
  {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++)
  {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
  file.close();
}
