#include <Arduino.h>
#include "SPI.h"
#include <lvgl.h>
#include "st7735.h"

#define SPI_MOSI 2
#define SPI_MISO 7
#define SPI_SCK 6

#define TFT_HOR_RES 80
#define TFT_VER_RES 160

#define PIN_LCD_MOSI SPI_MOSI
#define PIN_LCD_SCK SPI_SCK
#define PIN_LCD_BL 0
#define PIN_LCD_RST 1
#define PIN_LCD_DC 3
#define PIN_LCD_CS 10

Adafruit_ST7735 tft = Adafruit_ST7735(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_SCK, PIN_LCD_MOSI);
#define DISP_BUF_SIZE (240 * 40) // 双缓冲每帧40行
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];

lv_obj_t *src;
lv_obj_t *label;

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lvgl_init();

void setup()
{
  delay(500); // power-up safety delay
  Serial.begin(115200);
  Serial.println("Hello ESP32C5!");

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(0x001f); // 纯绿色

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, 0);

  String LVGL_Arduino = "Hello Arduino! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);
  lv_init();
  lvgl_init();

  /* 创建一个简单的UI界面作为测试 */
  src = lv_obj_create(lv_scr_act());
  lv_obj_set_style_bg_color(src, lv_color_hex(0x0000ff), 0);
  lv_obj_set_size(src, TFT_VER_RES, TFT_HOR_RES);
  label = lv_label_create(src);
  lv_label_set_text(label, "ESP32-C5 !");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);

  Serial.println("LVGL and ST7735 initialization complete.");
}

void loop()
{
  static uint32_t last_time = 0;
  static int count = 0;
  if (millis() - last_time > 2000)
  {
    last_time = millis();
    count++;
    lv_label_set_text_fmt(label, "T-Dongle-C5 #%d", count);
    lv_obj_set_style_bg_color(src, lv_color_hex(0x0000ff + (count * 0x001f) % 0xffff), 0);
  }

  lv_task_handler();
  delay(5);
}

void lvgl_init()
{
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.draw_buf = &draw_buf;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.hor_res = 160;
  disp_drv.ver_res = 80;
  lv_disp_drv_register(&disp_drv);
}

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  // Serial.printf("x1:%d,x2:%d,y1:%d,y2:%d\n",area->x1, area->y1, area->x2, area->y2);
  tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);

  tft.startWrite();
  tft.writePixels((uint16_t *)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp_drv);
}
