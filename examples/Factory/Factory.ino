#include <Arduino.h>
#include "pin_config.h"
#include "SPI.h"
#include <lvgl.h>
#include "st7735.h"
#include "FS.h"
#include "SD.h"
#include <APA102.h>
#include "LilyGo_Button.h"

#define APA102_LED_NUMBERS 1
APA102<LED_DI_PIN, LED_CI_PIN> ledStrip;
LilyGo_Button button;

bool SD_Mount = false;
Adafruit_ST7735 tft = Adafruit_ST7735(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_SCK, PIN_LCD_MOSI);

// LVGL显示缓冲
#define DISP_BUF_SIZE (240 * 40) // 双缓冲每帧40行
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];

lv_obj_t *src;
lv_obj_t *label;
lv_obj_t *sd_size_label;
lv_obj_t *sd_type_label;
lv_obj_t *sd_status_label;

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lvgl_init();
void led_task(void *pvParameters);
void lvgl_task(void *pvParameters);
void testFileIO(fs::FS &fs, const char *path);
void createDir(fs::FS &fs, const char *path);
void removeDir(fs::FS &fs, const char *path);
void readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void appendFile(fs::FS &fs, const char *path, const char *message);
void renameFile(fs::FS &fs, const char *path1, const char *path2);
void deleteFile(fs::FS &fs, const char *path);
void testFileIO(fs::FS &fs, const char *path);

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

  xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 2, NULL);
}

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  // Serial.printf("x1:%d,x2:%d,y1:%d,y2:%d\n", area->x1, area->y1, area->x2, area->y2);
  tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);

  // 批量传输像素数据
  tft.startWrite();
  tft.writePixels((uint16_t *)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp_drv);
}

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

void boot_btn_cb(ButtonState event)
{
  switch (event)
  {
  case BTN_CLICK_EVENT:
    Serial.println("boot btn pressed");
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Hello ESP32C5!");
  button.init(BOOT_BTN, 50, nullptr);
  button.setEventCallback(boot_btn_cb);
  pinMode(LED_CI_PIN, OUTPUT);
  pinMode(LED_DI_PIN, OUTPUT);

  SPI.begin(SD_CLK_PIN, SD_DAT0_PIN, SD_CMD_PIN, SD_CS_PIN);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, 0);

  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("Card Mount Failed");
    SD_Mount = false;
  }
  else
  {
    SD_Mount = true;
  }
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(0x0000); // blue background

  lv_init();
  lvgl_init();

  // 创建主屏幕对象
  src = lv_obj_create(lv_scr_act());
  lv_obj_set_size(src, TFT_VER_RES, TFT_HOR_RES);
  lv_obj_set_style_bg_color(src, lv_color_hex(0x000000), 0); // 黑色背景
  lv_obj_set_style_border_width(src, 0, 0);                  // 移除边框
  lv_obj_set_style_pad_all(src, 5, 0);                       // 设置内边距

  label = lv_label_create(src);
  lv_label_set_text(label, "T-Dongle-C5");
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

  sd_status_label = lv_label_create(src);
  lv_label_set_text(sd_status_label, SD_Mount ? "SD: Mounted" : "SD: Not Mounted");
  lv_obj_set_style_text_color(sd_status_label, SD_Mount ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_text_font(sd_status_label, &lv_font_montserrat_12, 0);
  lv_obj_align_to(sd_status_label, label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

  sd_type_label = lv_label_create(src);
  lv_label_set_text(sd_type_label, "Type: --");
  lv_obj_set_style_text_color(sd_type_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(sd_type_label, &lv_font_montserrat_12, 0);
  lv_obj_align_to(sd_type_label, sd_status_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

  sd_size_label = lv_label_create(src);
  lv_label_set_text(sd_size_label, "Size: -- MB");
  lv_obj_set_style_text_color(sd_size_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(sd_size_label, &lv_font_montserrat_12, 0);
  lv_obj_align_to(sd_size_label, sd_type_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

  if (SD_Mount)
  {
    uint8_t cardType = SD.cardType();
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

    // 更新 SD 卡类型标签
    const char *type_str = "UNKNOWN";
    if (cardType == CARD_MMC)
      type_str = "MMC";
    else if (cardType == CARD_SD)
      type_str = "SDSC";
    else if (cardType == CARD_SDHC)
      type_str = "SDHC";

    lv_label_set_text_fmt(sd_type_label, "Type: %s", type_str);

    lv_label_set_text_fmt(sd_size_label, "Size: %lluMB", cardSize);
  }

  xTaskCreate(led_task, "led_task", 4096, NULL, 1, NULL);
}

void led_task(void *pvParameters)
{
  Serial.println("LED Task");
  while (1)
  {
    button.update();

    unsigned long time = millis();
    float rawRed = (sin(time / 3000.0 * 2 * PI) + 1) / 2;
    float rawGreen = (sin(time / 3000.0 * 2 * PI + 2) + 1) / 2;
    float rawBlue = (sin(time / 3000.0 * 2 * PI + 4) + 1) / 2;

    uint8_t red = (uint8_t)(pow(rawRed, 2.2) * 255);
    uint8_t green = (uint8_t)(pow(rawGreen, 2.2) * 255);
    uint8_t blue = (uint8_t)(pow(rawBlue, 2.2) * 255);

    ledStrip.startFrame();
    ledStrip.sendColor(red, green, blue, 10);
    ledStrip.endFrame(1);
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void lvgl_task(void *pvParameters)
{
  Serial.println("LVGL Task");
  while (1)
  {
    lv_task_handler();
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void loop()
{
}
