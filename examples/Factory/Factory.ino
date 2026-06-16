#include <Arduino.h>
#include "pin_config.h"
#include "SPI.h"
#include <lvgl.h>
#include "st7735.h"
#include "FS.h"
#include "SD.h"
#include <APA102.h>
#include "LilyGo_Button.h"
#include "WiFi.h"

#define WIFI_SSID "xinyuandianzi"
#define WIFI_PASSWORD "AA15994823428"

#define APA102_LED_NUMBERS 1
APA102<LED_DI_PIN, LED_CI_PIN> ledStrip;
LilyGo_Button button;

bool SD_Mount = false;
uint64_t cardSize = 0;
Adafruit_ST7735 tft = Adafruit_ST7735(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_SCK, PIN_LCD_MOSI);

#define DISP_BUF_SIZE (160 * 80)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];

lv_obj_t *src;
lv_obj_t *label;
lv_obj_t *sd_size_label;
lv_obj_t *sd_type_label;
lv_obj_t *sd_status_label;
lv_obj_t *wifi_status_label;
lv_obj_t *wifi_rssi_label;

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lvgl_init();
void led_task(void *pvParameters);
void lvgl_task(void *pvParameters);
void sd_test_task(void *pvParameters);
void lvgl_update_cb(lv_timer_t *timer);
void readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);

void lvgl_init()
{
  lv_init();

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.draw_buf = &draw_buf;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.hor_res = 160;
  disp_drv.ver_res = 80;
  disp_drv.full_refresh = 1; // 强制全屏刷新，消除局部撕裂
  lv_disp_drv_register(&disp_drv);

  xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 2, NULL);
}

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);
  tft.writePixels((uint16_t *)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp_drv);
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

void boot_btn_cb(ButtonState event)
{
  static bool lcd_on = true;
  switch (event)
  {
  case BTN_CLICK_EVENT:
    Serial.println("boot btn pressed");
    lcd_on = !lcd_on;
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, lcd_on ? 0 : 1);
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

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  pinMode(PIN_LCD_CS, OUTPUT);
  digitalWrite(PIN_LCD_CS, HIGH);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, 0);

  SPI.begin(SD_CLK_PIN, SD_DAT0_PIN, SD_CMD_PIN);
  if (!SD.begin(SD_CS_PIN))
  {
    Serial.println("Card Mount Failed");
    SD_Mount = false;
  }
  else
  {
    SD_Mount = true;
  }
  delay(50);
  if (SD_Mount)
  {
    xTaskCreate(sd_test_task, "sd_test", 4096, NULL, 1, NULL);
  }

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(0x0000); // blue background

  lvgl_init();

  src = lv_obj_create(lv_scr_act());
  lv_obj_set_size(src, TFT_VER_RES, TFT_HOR_RES);
  lv_obj_set_style_bg_color(src, lv_color_hex(0x000000), 0); // 黑色背景
  lv_obj_set_style_border_width(src, 0, 0);                  // 移除边框
  lv_obj_set_style_pad_all(src, 5, 0);                       // 设置内边距

  label = lv_label_create(src);
  lv_label_set_text(label, "T-Dongle-C5");
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

  sd_status_label = lv_label_create(src);
  lv_label_set_text(sd_status_label, SD_Mount ? "SD: Mounted" : "SD: Not Mounted");
  lv_obj_set_style_text_color(sd_status_label, SD_Mount ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_text_font(sd_status_label, &lv_font_montserrat_12, 0);
  lv_obj_align(sd_status_label, LV_ALIGN_TOP_MID, 0, 13);

  sd_size_label = lv_label_create(src);
  lv_label_set_text(sd_size_label, "Size: -- MB");
  lv_obj_set_style_text_color(sd_size_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(sd_size_label, &lv_font_montserrat_12, 0);
  lv_obj_align(sd_size_label, LV_ALIGN_TOP_MID, 0, 13 * 2);

  wifi_status_label = lv_label_create(src);
  lv_label_set_text(wifi_status_label, "WiFi: --");
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_12, 0);
  lv_obj_align(wifi_status_label, LV_ALIGN_TOP_MID, 0, 13 * 3);

  wifi_rssi_label = lv_label_create(src);
  lv_label_set_text(wifi_rssi_label, "Rssi: -- dBm");
  lv_obj_set_style_text_color(wifi_rssi_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(wifi_rssi_label, &lv_font_montserrat_12, 0);
  lv_obj_align(wifi_rssi_label, LV_ALIGN_TOP_MID, 0, 13 * 4);

  lv_timer_create(lvgl_update_cb, 1000, NULL);

  xTaskCreate(led_task, "led_task", 4096, NULL, 1, NULL);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    lv_label_set_text_fmt(wifi_status_label, "%s", WIFI_SSID);
  }
  else
  {
    Serial.println("\nWiFi connection failed");
    lv_label_set_text(wifi_status_label, "WiFi: DisConnected");
  }
}

void loop()
{
  vTaskDelay(10000 / portTICK_PERIOD_MS);
}

void lvgl_update_cb(lv_timer_t *timer)
{
  lv_label_set_text_fmt(wifi_rssi_label, "Rssi: %d dBm", WiFi.RSSI());
  lv_label_set_text_fmt(sd_size_label, "Size: %lluMB", cardSize);
}

void sd_test_task(void *pvParameters)
{
  uint16_t count = 0;
  while (1)
  {
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    Serial.println("SD Test Task started");
    cardSize = SD.cardSize() / (1024 * 1024);
    count++;
    char msg[50];
    sprintf(msg, "T-Dongle-C5 SD Test #%d", count);
    writeFile(SD, "/test.txt", msg);
    readFile(SD, "/test.txt");

    Serial.printf("\nTotal space: %.2fMB\n", (float)SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %.2fMB\n", (float)SD.usedBytes() / (1024 * 1024));
  }
  vTaskDelete(NULL);
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
  vTaskDelete(NULL);
}

void lvgl_task(void *pvParameters)
{
  Serial.println("LVGL Task");
  while (1)
  {
    lv_task_handler();
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}
