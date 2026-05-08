/**
 * T-Dongle-C5 WiFi Serial Bridge (5GHz)
 *
 * Creates a WiFi AP on 5GHz band, listens for TCP connections,
 * bridges Serial <-> TCP data, displays messages on LCD via LVGL.
 */

#include <Arduino.h>
#include "pin_config.h"
#include "SPI.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include "st7735.h"
#include <APA102.h>
#include "LilyGo_Button.h"

// ========== WiFi Configuration ==========
#define WIFI_SSID       "T-Dongle-C5"
#define WIFI_PASSWORD   "12345678"
#define WIFI_CHANNEL_5G 149     // 5GHz channel (36-165)
#define TCP_PORT        8888
#define MAX_CLIENTS     1

#define BOOT_BTN        28

// ========== APA102 RGB LED ==========
#define APA102_LED_NUMBERS 1
APA102<LED_DI_PIN, LED_CI_PIN> ledStrip;

// ========== Button ==========
LilyGo_Button button;

// ========== LCD ==========
Adafruit_ST7735 tft = Adafruit_ST7735(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_SCK, PIN_LCD_MOSI);

// ========== LVGL ==========
#define DISP_BUF_SIZE (240 * 40)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];
static lv_disp_drv_t disp_drv;

// ========== UI Objects ==========
lv_obj_t *status_label;
lv_obj_t *ip_label;
lv_obj_t *client_label;
lv_obj_t *msg_cont;
lv_obj_t *msg_labels[5];
lv_obj_t *clear_btn;

// ========== Message Ring Buffer ==========
#define MAX_MESSAGES    20
#define MSG_LINE_LEN    22

typedef struct {
    char text[MSG_LINE_LEN];
    bool is_rx;
} MessageEntry;

MessageEntry msg_buffer[MAX_MESSAGES];
int msg_head = 0;
int msg_count = 0;

// ========== WiFi ==========
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;
bool client_connected = false;

// LED state machine
enum LedState {
    LED_WAITING,
    LED_CONNECTED,
    LED_DATA_RX,
    LED_DATA_TX,
    LED_ERROR
};
LedState led_state = LED_WAITING;
uint32_t led_flash_until = 0;

// ========== Function Declarations ==========
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lvgl_init();
void ui_init();
void add_message(const char *text, bool is_rx);
void update_msg_display();
void set_led(LedState state);
void led_task(void *pvParameters);
void lvgl_task(void *pvParameters);
void wifi_task(void *pvParameters);
void btn_callback(ButtonState event);

// =====================================================================
//  LVGL Display Driver
// =====================================================================
void my_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);
    tft.startWrite();
    tft.writePixels((uint16_t *)color_p, w * h);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

void lvgl_init()
{
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.hor_res = 160;
    disp_drv.ver_res = 80;
    lv_disp_drv_register(&disp_drv);

    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 2, NULL);
}

// =====================================================================
//  UI
// =====================================================================
void ui_init()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);

    // Status line
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "AP:5GHz");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(status_label, 2, 0);

    ip_label = lv_label_create(scr);
    lv_label_set_text(ip_label, "IP:--");
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(ip_label, &lv_font_unscii_8, 0);
    lv_obj_set_pos(ip_label, 2, 8);

    client_label = lv_label_create(scr);
    lv_label_set_text(client_label, "Clients:0");
    lv_obj_set_style_text_color(client_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(client_label, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_letter_space(client_label, 0, 0);
    lv_obj_set_pos(client_label, 80, 0);

    // Separator
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 156, 1);
    lv_obj_set_pos(sep, 2, 18);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);

    // Message container
    msg_cont = lv_obj_create(scr);
    lv_obj_set_size(msg_cont, 158, 42);
    lv_obj_set_pos(msg_cont, 1, 20);
    lv_obj_set_style_bg_color(msg_cont, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(msg_cont, 0, 0);
    lv_obj_set_style_pad_all(msg_cont, 0, 0);

    // Pre-create 5 message label slots
    for (int i = 0; i < 5; i++) {
        msg_labels[i] = lv_label_create(msg_cont);
        lv_label_set_text(msg_labels[i], "");
        lv_obj_set_style_text_font(msg_labels[i], &lv_font_unscii_8, 0);
        lv_obj_set_pos(msg_labels[i], 2, 1 + i * 8);
    }

    // Clear button
    clear_btn = lv_btn_create(scr);
    lv_obj_set_size(clear_btn, 28, 10);
    lv_obj_set_pos(clear_btn, 130, 64);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(clear_btn, 2, 0);

    lv_obj_t *btn_lbl = lv_label_create(clear_btn);
    lv_label_set_text(btn_lbl, "CLR");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_unscii_8, 0);
    lv_obj_center(btn_lbl);

    lv_obj_add_event_cb(clear_btn, [](lv_event_t *e) {
        msg_head = 0;
        msg_count = 0;
        for (int i = 0; i < 5; i++) {
            lv_label_set_text(msg_labels[i], "");
        }
    }, LV_EVENT_CLICKED, NULL);
}

// =====================================================================
//  Message Buffer
// =====================================================================
void add_message(const char *text, bool is_rx)
{
    size_t maxcopy = MSG_LINE_LEN - 3;

    int idx;
    if (msg_count < MAX_MESSAGES) {
        idx = (msg_head + msg_count) % MAX_MESSAGES;
        msg_count++;
    } else {
        idx = msg_head;
        msg_head = (msg_head + 1) % MAX_MESSAGES;
    }

    msg_buffer[idx].text[0] = is_rx ? 'R' : 'T';
    msg_buffer[idx].text[1] = ':';
    size_t slen = strlen(text);
    size_t copylen = slen < maxcopy ? slen : maxcopy - 1;
    memcpy(msg_buffer[idx].text + 2, text, copylen);
    msg_buffer[idx].text[2 + copylen] = '\0';
    msg_buffer[idx].is_rx = is_rx;

    update_msg_display();
}

void update_msg_display()
{
    int show = msg_count < 5 ? msg_count : 5;
    int start = msg_count < 5 ? 0 : (msg_head + msg_count - 5) % MAX_MESSAGES;

    // Adjust start to show the last `show` messages
    if (msg_count >= 5) {
        start = (msg_head + msg_count - 5) % MAX_MESSAGES;
    } else {
        start = msg_head;
    }

    for (int i = 0; i < 5; i++) {
        if (i < show) {
            int idx = (start + i) % MAX_MESSAGES;
            lv_label_set_text(msg_labels[i], msg_buffer[idx].text);
            lv_obj_set_style_text_color(msg_labels[i],
                msg_buffer[idx].is_rx ? lv_color_hex(0x00FF00) : lv_color_hex(0xFFCC00), 0);
            lv_obj_clear_flag(msg_labels[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(msg_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// =====================================================================
//  RGB LED Control
// =====================================================================
void set_led(LedState state)
{
    led_state = state;
    switch (state) {
        case LED_WAITING:   ledStrip.sendColor(0, 0, 255, 5); break;
        case LED_CONNECTED: ledStrip.sendColor(0, 255, 0, 5); break;
        case LED_DATA_RX:   ledStrip.sendColor(0, 255, 255, 8); break;
        case LED_DATA_TX:   ledStrip.sendColor(255, 255, 0, 8); break;
        case LED_ERROR:     ledStrip.sendColor(255, 0, 0, 10); break;
    }
}

// =====================================================================
//  Button Callback
// =====================================================================
void btn_callback(ButtonState event)
{
    if (event == BTN_CLICK_EVENT) {
        msg_head = 0;
        msg_count = 0;
        for (int i = 0; i < 5; i++) {
            lv_label_set_text(msg_labels[i], "");
        }
    }
}

// =====================================================================
//  WiFi Setup (5GHz)
// =====================================================================
void setup_wifi_5g()
{
    WiFi.mode(WIFI_AP);

    // Set country code to enable full 5GHz channel range
    wifi_country_t country = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    esp_err_t cerr = esp_wifi_set_country(&country);
    Serial.printf("Set country CN: %s\n", cerr == ESP_OK ? "OK" : "FAIL");
    delay(100);

    // Try 5GHz channel 149
    bool ap_ok = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL_5G, 0, MAX_CLIENTS);

    // Fallback: try a lower 5GHz channel
    if (!ap_ok) {
        Serial.println("CH149 failed, trying CH36...");
        ap_ok = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, 36, 0, MAX_CLIENTS);
    }

    // Fallback: 2.4GHz
    if (!ap_ok) {
        Serial.println("5GHz failed, falling back to 2.4GHz CH6");
        WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, 6, 0, MAX_CLIENTS);
    }
    delay(200);

    uint8_t actual_ch = WiFi.channel();
    IPAddress ip = WiFi.softAPIP();

    Serial.printf("AP: SSID=%s CH=%d IP=%s\n",
                  WIFI_SSID, actual_ch, ip.toString().c_str());

    // Update UI
    lv_label_set_text_fmt(ip_label, "IP:%s", ip.toString().c_str());
    if (actual_ch >= 36) {
        lv_label_set_text_fmt(status_label, "5G CH:%d", actual_ch);
    } else {
        lv_label_set_text_fmt(status_label, "2.4G CH:%d", actual_ch);
    }
}

// =====================================================================
//  FreeRTOS Tasks
// =====================================================================
void led_task(void *pvParameters)
{
    ledStrip.startFrame();
    set_led(LED_WAITING);
    ledStrip.endFrame(APA102_LED_NUMBERS);

    while (1) {
        button.update();

        ledStrip.startFrame();
        if (millis() < led_flash_until) {
            switch (led_state) {
                case LED_DATA_RX: ledStrip.sendColor(0, 255, 255, 8); break;
                case LED_DATA_TX: ledStrip.sendColor(255, 255, 0, 8); break;
                default:          set_led(led_state); break;
            }
        } else if (client_connected) {
            set_led(LED_CONNECTED);
        } else {
            set_led(LED_WAITING);
        }
        ledStrip.endFrame(APA102_LED_NUMBERS);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void lvgl_task(void *pvParameters)
{
    while (1) {
        lv_task_handler();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void wifi_task(void *pvParameters)
{
    while (1) {
        // Accept new client
        if (tcpServer.hasClient()) {
            if (!tcpClient || !tcpClient.connected()) {
                tcpClient = tcpServer.available();
                tcpClient.setNoDelay(true);
                client_connected = true;
                lv_label_set_text_fmt(client_label, "Clients:1");
                Serial.println("Client connected!");
                add_message("Client connected", true);
                led_flash_until = millis() + 200;
                led_state = LED_DATA_RX;
            } else {
                tcpServer.available().stop();
            }
        }

        // Check client disconnection
        if (client_connected && (!tcpClient || !tcpClient.connected())) {
            client_connected = false;
            lv_label_set_text_fmt(client_label, "Clients:0");
            Serial.println("Client disconnected");
            add_message("Client disconnected", true);
        }

        // TCP -> Serial
        if (tcpClient && tcpClient.connected() && tcpClient.available()) {
            String data = tcpClient.readStringUntil('\n');
            data.replace("\r", "");
            data.trim();
            if (data.length() > 0) {
                Serial.println(data);
                add_message(data.c_str(), true);
                led_state = LED_DATA_RX;
                led_flash_until = millis() + 150;
            }
        }

        // Serial -> TCP
        if (Serial.available()) {
            String data;
            while (Serial.available()) {
                char c = Serial.read();
                if (c == '\r') continue;
                if (c == '\n') break;
                data += c;
                delay(1);
            }
            data.trim();
            if (data.length() > 0) {
                if (tcpClient && tcpClient.connected()) {
                    tcpClient.println(data);
                    led_state = LED_DATA_TX;
                    led_flash_until = millis() + 150;
                }
                add_message(data.c_str(), false);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// =====================================================================
//  Arduino Entry Points
// =====================================================================
void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== T-Dongle-C5 WiFi Serial Bridge (5GHz) ===");

    button.init(BOOT_BTN, 50, NULL);
    button.setEventCallback(btn_callback);

    pinMode(LED_DI_PIN, OUTPUT);
    pinMode(LED_CI_PIN, OUTPUT);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, 0);

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(0x0000);

    lv_init();
    lvgl_init();

    ui_init();
    setup_wifi_5g();

    tcpServer.begin();
    Serial.printf("TCP server on port %d\n", TCP_PORT);

    xTaskCreate(led_task, "led", 2048, NULL, 1, NULL);
    xTaskCreate(wifi_task, "wifi", 4096, NULL, 1, NULL);

    Serial.println("=== System ready ===");
}

void loop()
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
