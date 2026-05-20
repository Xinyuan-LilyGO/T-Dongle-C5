#include <Arduino.h>
#include "pin_config.h"
#include "SPI.h"
#include <lvgl.h>
#include "st7735.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_netif.h>

// ===================== AP 配置 =====================
#define AP_SSID        "ESP32-C5-Repeater"
#define AP_PASSWORD    "12345678"
#define AP_CHANNEL     6

// ===================== DNS/Web 配置 =====================
#define DNS_PORT        53
#define WEB_PORT        80
#define CAPTIVE_TIMEOUT 300  // DNS超时缓存(秒)

// ===================== LVGL 显示缓冲 =====================
#define DISP_BUF_SIZE (240 * 40)
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];

// ===================== 全局对象 =====================
Adafruit_ST7735 tft = Adafruit_ST7735(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RST, PIN_LCD_SCK, PIN_LCD_MOSI);
WebServer server(WEB_PORT);
DNSServer dnsServer;
Preferences prefs;

// ===================== LVGL 对象 =====================
static lv_obj_t *scr;
static lv_obj_t *title_label;
static lv_obj_t *sta_status_label;
static lv_obj_t *sta_ssid_label;
static lv_obj_t *ap_label;
static lv_obj_t *rate_label;
static lv_obj_t *ip_label;

// ===================== 状态变量 =====================
static String sta_ssid = "";
static String sta_password = "";
static bool sta_connected = false;
static int wifi_phy_rate = 0;
static unsigned long last_status_update = 0;
static unsigned long last_scan_update = 0;
static String saved_ssid = "";
static String saved_password = "";

// ===================== 前置声明 =====================
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lvgl_init();
void lvgl_task(void *pvParameters);
void start_ap();
bool connect_to_wifi(const char *ssid, const char *password);
void update_display();
void handle_root();
void handle_scan();
void handle_connect();
void handle_status();
void handle_disconnect();
void configure_ap_dhcp_dns(const char *dns_ip);

// ===================== LVGL 显示初始化 =====================
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
  xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 2, NULL);
}

void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.setAddrWindow(area->x1, area->y1, area->x2, area->y2);
  tft.startWrite();
  tft.writePixels((uint16_t *)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp_drv);
}

void lvgl_task(void *pvParameters)
{
  while (1) {
    lv_task_handler();
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

// ===================== UI 创建 =====================
void create_ui()
{
  scr = lv_obj_create(lv_scr_act());
  lv_obj_set_size(scr, 160, 80);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a0a1e), 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 4, 0);

  // 标题栏
  title_label = lv_label_create(scr);
  lv_label_set_text(title_label, "WiFi Repeater");
  lv_obj_set_style_text_color(title_label, lv_color_hex(0x00e5ff), 0);
  lv_obj_set_style_text_font(title_label, &lv_font_unscii_8, 0);
  lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

  // STA 连接状态
  sta_status_label = lv_label_create(scr);
  lv_label_set_text(sta_status_label, "STA: --");
  lv_obj_set_style_text_color(sta_status_label, lv_color_hex(0x888888), 0);
  lv_obj_set_style_text_font(sta_status_label, &lv_font_unscii_8, 0);
  lv_obj_align(sta_status_label, LV_ALIGN_TOP_LEFT, 0, 12);

  // STA SSID
  sta_ssid_label = lv_label_create(scr);
  lv_label_set_text(sta_ssid_label, "");
  lv_obj_set_style_text_color(sta_ssid_label, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(sta_ssid_label, &lv_font_unscii_8, 0);
  lv_obj_align(sta_ssid_label, LV_ALIGN_TOP_LEFT, 0, 24);

  // AP 信息
  ap_label = lv_label_create(scr);
  lv_label_set_text(ap_label, "AP: " AP_SSID);
  lv_obj_set_style_text_color(ap_label, lv_color_hex(0x00ff88), 0);
  lv_obj_set_style_text_font(ap_label, &lv_font_unscii_8, 0);
  lv_obj_align(ap_label, LV_ALIGN_TOP_LEFT, 0, 36);

  // 速率
  rate_label = lv_label_create(scr);
  lv_label_set_text(rate_label, "Rate: -- Mbps");
  lv_obj_set_style_text_color(rate_label, lv_color_hex(0xffcc00), 0);
  lv_obj_set_style_text_font(rate_label, &lv_font_unscii_8, 0);
  lv_obj_align(rate_label, LV_ALIGN_TOP_LEFT, 0, 48);

  // IP 地址
  ip_label = lv_label_create(scr);
  lv_label_set_text(ip_label, "IP: --");
  lv_obj_set_style_text_color(ip_label, lv_color_hex(0xaaaaaa), 0);
  lv_obj_set_style_text_font(ip_label, &lv_font_unscii_8, 0);
  lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, 0, 60);
}

// ===================== AP DHCP DNS 配置 =====================
void configure_ap_dhcp_dns(const char *dns_ip)
{
  esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (!ap_netif) {
    Serial.println("[DHCP] Failed to get AP netif");
    return;
  }

  esp_netif_dns_info_t dns;
  dns.ip.type = IPADDR_TYPE_V4;
  dns.ip.u_addr.ip4.addr = esp_ip4addr_aton(dns_ip);
  esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns);
  Serial.printf("[DHCP] AP DNS set to %s\n", dns_ip);
}

// ===================== WiFi NAT 设置 =====================
void setup_nat()
{
  esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

  if (ap_netif && sta_netif) {
    esp_netif_napt_enable(ap_netif);
    Serial.println("[NAT] Enabled on AP interface");

    // STA连接后切换AP DHCP DNS为公网DNS，让客户端可上网
    configure_ap_dhcp_dns("8.8.8.8");

    // 停止DNS劫持，允许真实DNS解析经过NAT
    dnsServer.stop();
    Serial.println("[DNS] Captive portal stopped, internet access enabled");
  } else {
    Serial.println("[NAT] Failed to get netif handles");
  }
}

// ===================== WiFi 连接 =====================
bool connect_to_wifi(const char *ssid, const char *password)
{
  Serial.printf("[WiFi] Connecting to %s...\n", ssid);
  lv_label_set_text(sta_status_label, "Connecting...");
  lv_obj_set_style_text_color(sta_status_label, lv_color_hex(0xffaa00), 0);
  lv_label_set_text(sta_ssid_label, ssid);

  WiFi.disconnect(false);
  delay(200);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    sta_ssid = String(ssid);
    sta_password = String(password);
    sta_connected = true;

    // 保存凭证
    prefs.putString("sta_ssid", ssid);
    prefs.putString("sta_pwd", password);

    lv_label_set_text(sta_status_label, "STA: Connected");
    lv_obj_set_style_text_color(sta_status_label, lv_color_hex(0x00ff00), 0);

    return true;
  } else {
    Serial.println("\n[WiFi] Connection failed");
    sta_connected = false;
    lv_label_set_text(sta_status_label, "STA: Failed");
    lv_obj_set_style_text_color(sta_status_label, lv_color_hex(0xff4444), 0);
    return false;
  }
}

// ===================== AP 启动 =====================
void start_ap()
{
  WiFi.mode(WIFI_AP_STA);

  // 启动AP
  bool ap_ok = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, 4);
  if (ap_ok) {
    Serial.println("[AP] Started: " AP_SSID);
    lv_label_set_text(ap_label, "AP: " AP_SSID);
    lv_obj_set_style_text_color(ap_label, lv_color_hex(0x00ff88), 0);
    lv_label_set_text_fmt(ip_label, "AP IP: %s", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[AP] Failed to start");
    lv_label_set_text(ap_label, "AP: ERROR");
    lv_obj_set_style_text_color(ap_label, lv_color_hex(0xff0000), 0);
  }
}

// ===================== 更新 LCD 显示 =====================
void update_display()
{
  if (!sta_connected) return;

  // 获取WiFi速率
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    lv_label_set_text_fmt(sta_ssid_label, "%s", ap_info.ssid);
    lv_label_set_text_fmt(rate_label, "Rate: -- Mbps");
  }

  // 尝试获取PHY速率
  wifi_phy_rate = 0;
  int rssi = WiFi.RSSI();

  // 根据RSSI估算连接速率
  int estimated_rate = 0;
  if (rssi > -50)
    estimated_rate = 72;
  else if (rssi > -60)
    estimated_rate = 58;
  else if (rssi > -70)
    estimated_rate = 36;
  else if (rssi > -80)
    estimated_rate = 18;
  else
    estimated_rate = 6;

  lv_label_set_text_fmt(rate_label, "Rate:~%dM %ddBm", estimated_rate, rssi);
  if (rssi > -60)
    lv_obj_set_style_text_color(rate_label, lv_color_hex(0x00ff00), 0);
  else if (rssi > -75)
    lv_obj_set_style_text_color(rate_label, lv_color_hex(0xffcc00), 0);
  else
    lv_obj_set_style_text_color(rate_label, lv_color_hex(0xff4444), 0);
}

// ===================== Web 服务器处理函数 =====================
void handle_root()
{
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>WiFi Repeater</title>
<style>
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
  body{
    font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
    background:linear-gradient(135deg,#0a0a1e 0%,#1a1040 50%,#0a0a1e 100%);
    min-height:100vh;color:#e0e0e0;
    display:flex;justify-content:center;align-items:flex-start;
    padding:16px;
  }
  .container{
    width:100%;max-width:420px;
  }
  .card{
    background:rgba(255,255,255,0.04);
    backdrop-filter:blur(20px);
    -webkit-backdrop-filter:blur(20px);
    border:1px solid rgba(255,255,255,0.08);
    border-radius:20px;padding:24px;
    margin-bottom:16px;
    box-shadow:0 8px 32px rgba(0,0,0,0.3);
  }
  .header{
    text-align:center;margin-bottom:8px;
  }
  .header .icon{
    width:48px;height:48px;margin:0 auto 12px;
    background:linear-gradient(135deg,#00e5ff,#00b8d4);
    border-radius:14px;display:flex;align-items:center;justify-content:center;
    font-size:24px;color:#0a0a1e;
  }
  h1{
    font-size:20px;font-weight:700;
    background:linear-gradient(90deg,#00e5ff,#00ff88);
    -webkit-background-clip:text;-webkit-text-fill-color:transparent;
    background-clip:text;
    margin-bottom:6px;
  }
  .subtitle{font-size:12px;color:#666;margin-bottom:4px;}

  /* 状态指示器 */
  .status-row{
    display:flex;align-items:center;gap:8px;
    padding:10px 14px;border-radius:12px;
    background:rgba(255,255,255,0.03);
    margin-bottom:16px;
  }
  .status-dot{
    width:10px;height:10px;border-radius:50%;
    flex-shrink:0;
    animation:pulse 2s infinite;
  }
  .status-dot.online{background:#00e676;box-shadow:0 0 8px #00e676;}
  .status-dot.offline{background:#ff5252;box-shadow:0 0 8px #ff5252;}
  @keyframes pulse{
    0%,100%{opacity:1}50%{opacity:0.4}
  }
  .status-text{font-size:13px;font-weight:500;}
  .status-text small{display:block;font-size:11px;color:#888;font-weight:400;}

  /* 表单 */
  label{
    display:block;font-size:11px;font-weight:600;color:#999;
    text-transform:uppercase;letter-spacing:0.5px;margin:12px 0 4px;
  }
  .input-group{
    position:relative;
  }
  .input-group .icon-prefix{
    position:absolute;left:14px;top:50%;transform:translateY(-50%);
    font-size:14px;color:#666;z-index:1;
  }
  input{
    width:100%;padding:12px 14px 12px 38px;
    background:rgba(255,255,255,0.06);
    border:1px solid rgba(255,255,255,0.1);
    border-radius:12px;color:#fff;font-size:14px;
    outline:none;transition:all 0.25s;
  }
  input:focus{
    border-color:#00e5ff;
    box-shadow:0 0 0 3px rgba(0,229,255,0.12);
    background:rgba(255,255,255,0.1);
  }
  input::placeholder{color:#555;}

  /* 按钮 */
  .btn-row{
    display:flex;gap:10px;margin-top:16px;
  }
  .btn{
    flex:1;padding:12px 16px;border:none;border-radius:12px;
    font-size:13px;font-weight:600;cursor:pointer;
    transition:all 0.2s;text-align:center;
    letter-spacing:0.3px;
  }
  .btn:active{transform:scale(0.97);}
  .btn-primary{
    background:linear-gradient(135deg,#00e5ff,#00b8d4);
    color:#0a0a1e;
  }
  .btn-primary:hover{box-shadow:0 4px 20px rgba(0,229,255,0.3);}
  .btn-outline{
    background:transparent;color:#00e5ff;
    border:1.5px solid rgba(0,229,255,0.4);
  }
  .btn-outline:hover{background:rgba(0,229,255,0.06);border-color:#00e5ff;}
  .btn-danger{
    background:rgba(255,82,82,0.15);color:#ff5252;
    border:1.5px solid rgba(255,82,82,0.3);
  }
  .btn-danger:hover{background:rgba(255,82,82,0.25);}

  /* WiFi list */
  .wifi-list{margin-top:10px;}
  .wifi-item{
    display:flex;align-items:center;gap:10px;
    padding:12px 14px;border-radius:12px;
    background:rgba(255,255,255,0.03);
    margin-bottom:8px;cursor:pointer;
    transition:all 0.2s;
    border:1px solid transparent;
  }
  .wifi-item:hover,.wifi-item.selected{
    background:rgba(0,229,255,0.06);
    border-color:rgba(0,229,255,0.25);
  }
  .wifi-item .signal{
    width:28px;height:28px;flex-shrink:0;
    border-radius:8px;display:flex;align-items:center;justify-content:center;
    font-size:14px;
  }
  .wifi-item .sig-4{background:rgba(0,230,118,0.2);color:#00e676;}
  .wifi-item .sig-3{background:rgba(0,229,255,0.2);color:#00e5ff;}
  .wifi-item .sig-2{background:rgba(255,204,0,0.2);color:#ffcc00;}
  .wifi-item .sig-1{background:rgba(255,152,0,0.2);color:#ff9800;}
  .wifi-item .sig-0{background:rgba(255,82,82,0.2);color:#ff5252;}
  .wifi-item .info{flex:1;min-width:0;}
  .wifi-item .info .name{font-size:13px;font-weight:500;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .wifi-item .info .sec{font-size:10px;color:#888;}
  .wifi-item .lock{font-size:11px;color:#888;}
  .wifi-item .arrow{color:#555;font-size:12px;}

  /* Toast */
  .toast{
    position:fixed;top:16px;left:50%;transform:translateX(-50%);
    padding:10px 20px;border-radius:12px;font-size:13px;font-weight:500;
    z-index:999;animation:slideDown 0.35s ease;
    backdrop-filter:blur(10px);
    -webkit-backdrop-filter:blur(10px);
    display:none;
  }
  .toast.success{background:rgba(0,230,118,0.85);color:#fff;}
  .toast.error{background:rgba(255,82,82,0.85);color:#fff;}
  .toast.info{background:rgba(0,229,255,0.85);color:#0a0a1e;}

  @keyframes slideDown{
    from{opacity:0;transform:translateX(-50%) translateY(-20px);}
    to{opacity:1;transform:translateX(-50%) translateY(0);}
  }

  /* 加载动画 */
  .spinner{
    display:inline-block;width:16px;height:16px;
    border:2px solid rgba(255,255,255,0.2);
    border-top-color:#fff;border-radius:50%;
    animation:spin 0.7s linear infinite;
    vertical-align:middle;margin-right:6px;
  }
  @keyframes spin{to{transform:rotate(360deg)}}

  /* 页脚 */
  .footer{
    text-align:center;font-size:10px;color:#444;margin-top:8px;
  }
</style>
</head>
<body>
<div class="container">
  <!-- 头部 -->
  <div class="card header">
    <div class="icon">&#9881;</div>
    <h1>WiFi Repeater</h1>
    <p class="subtitle">ESP32-C5 中继器配置</p>
  </div>

  <!-- 状态卡片 -->
  <div class="card">
    <div class="status-row">
      <div class="status-dot" id="statusDot"></div>
      <div class="status-text">
        <span id="statusText">检测中...</span>
        <small id="statusDetail"></small>
      </div>
      <button class="btn btn-outline" style="flex:none;padding:6px 14px;font-size:11px;margin-left:auto;" onclick="refreshStatus()">刷新</button>
    </div>
  </div>

  <!-- WiFi 列表 -->
  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;">
      <label style="margin:0;">附近 WiFi</label>
      <button class="btn btn-outline" style="flex:none;padding:6px 14px;font-size:11px;" onclick="scanWiFi()" id="scanBtn">扫描</button>
    </div>
    <div class="wifi-list" id="wifiList">
      <div style="text-align:center;color:#555;padding:20px;font-size:13px;">点击扫描搜索WiFi</div>
    </div>
  </div>

  <!-- 连接表单 -->
  <div class="card">
    <label>WiFi名称</label>
    <div class="input-group">
      <span class="icon-prefix">&#127760;</span>
      <input type="text" id="ssid" placeholder="输入WiFi名称或点击上方选择" autocomplete="off">
    </div>
    <label>WiFi密码</label>
    <div class="input-group">
      <span class="icon-prefix">&#128274;</span>
      <input type="password" id="password" placeholder="输入WiFi密码">
    </div>
    <div class="btn-row">
      <button class="btn btn-primary" onclick="connectWiFi()" id="connectBtn">连接</button>
      <button class="btn btn-danger" onclick="disconnectWiFi()" id="disconnectBtn" style="display:none;">断开</button>
    </div>
  </div>

  <div class="footer">ESP32-C5 Repeater &copy; 2025</div>
</div>

<!-- Toast -->
<div class="toast" id="toast" onclick="this.style.display='none'"></div>

<script>
  var currentSSID = '';
  var connected = false;
  var scanning = false;

  // Toast 提示
  function showToast(msg, type) {
    var t = document.getElementById('toast');
    t.textContent = msg;
    t.className = 'toast ' + (type || 'info');
    t.style.display = 'block';
    setTimeout(function(){ t.style.display = 'none'; }, 2500);
  }

  // 更新UI连接状态
  function updateConnectionUI() {
    var dot = document.getElementById('statusDot');
    var text = document.getElementById('statusText');
    var detail = document.getElementById('statusDetail');
    var cBtn = document.getElementById('connectBtn');
    var dBtn = document.getElementById('disconnectBtn');

    if (connected) {
      dot.className = 'status-dot online';
      text.textContent = '已连接 — ' + currentSSID;
      cBtn.textContent = '切换连接';
      dBtn.style.display = 'block';
    } else {
      dot.className = 'status-dot offline';
      if (currentSSID) {
        text.textContent = '未连接 — 上次: ' + currentSSID;
      } else {
        text.textContent = '未连接';
      }
      cBtn.textContent = '连接';
      dBtn.style.display = 'none';
    }
  }

  // 获取当前状态
  function refreshStatus() {
    fetch('/api/status')
      .then(function(r){ return r.json(); })
      .then(function(data) {
        connected = data.connected;
        currentSSID = data.ssid || '';
        updateConnectionUI();
        var detail = document.getElementById('statusDetail');
        if (data.connected) {
          detail.textContent = 'IP: ' + (data.ip || '--') + ' | RSSI: ' + (data.rssi || '--') + ' dBm | AP: ' + (data.ap_ssid || '--');
          document.getElementById('ssid').value = data.ssid;
        } else {
          detail.textContent = 'AP已开启: ' + (data.ap_ssid || '--');
        }
      })
      .catch(function() {
        showToast('获取状态失败','error');
      });
  }

  // 扫描WiFi
  function scanWiFi() {
    if (scanning) return;
    scanning = true;
    var btn = document.getElementById('scanBtn');
    btn.textContent = '扫描中...';
    btn.disabled = true;

    var list = document.getElementById('wifiList');
    list.innerHTML = '<div style="text-align:center;color:#888;padding:20px;"><span class="spinner"></span>扫描中...</div>';

    fetch('/api/scan')
      .then(function(r){ return r.json(); })
      .then(function(networks) {
        var html = '';
        networks.forEach(function(net, i) {
          var rssi = net.rssi;
          var sigLevel = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : rssi > -85 ? 1 : 0;
          var sigIcon = sigLevel >= 4 ? '▂▄▆█' : sigLevel >= 3 ? '▂▄▆_' : sigLevel >= 2 ? '▂▄__' : sigLevel >= 1 ? '▂___' : '____';
          html += '<div class="wifi-item" onclick="selectWiFi(\'' + net.ssid.replace(/'/g, "\\'") + '\', \'' + net.enc + '\', this)" data-ssid="' + net.ssid.replace(/"/g, '&quot;') + '" data-enc="' + net.enc + '">';
          html += '<div class="signal sig-' + sigLevel + '">' + sigIcon + '</div>';
          html += '<div class="info"><div class="name">' + net.ssid + '</div><div class="sec">' + (net.enc === 'OPEN' ? '开放网络' : 'WPA/WPA2') + ' &middot; Ch.' + net.ch + '</div></div>';
          html += net.enc !== 'OPEN' ? '<div class="lock">&#128274;</div>' : '';
          html += '<div class="arrow">&#8250;</div>';
          html += '</div>';
        });
        if (networks.length === 0) {
          html = '<div style="text-align:center;color:#555;padding:20px;">未发现WiFi网络</div>';
        }
        list.innerHTML = html;
        scanning = false;
        btn.textContent = '扫描';
        btn.disabled = false;
      })
      .catch(function() {
        showToast('扫描失败','error');
        scanning = false;
        btn.textContent = '扫描';
        btn.disabled = false;
      });
  }

  // 选择WiFi
  function selectWiFi(ssid, enc, el) {
    document.getElementById('ssid').value = ssid;
    // 高亮选中项
    var items = document.querySelectorAll('.wifi-item');
    items.forEach(function(item){ item.classList.remove('selected'); });
    el.classList.add('selected');

    if (enc === 'OPEN') {
      document.getElementById('password').value = '';
      document.getElementById('password').placeholder = '开放网络无需密码';
      document.getElementById('password').disabled = true;
    } else {
      document.getElementById('password').disabled = false;
      document.getElementById('password').placeholder = '输入WiFi密码';
      document.getElementById('password').focus();
    }
  }

  // 连接WiFi
  function connectWiFi() {
    var ssid = document.getElementById('ssid').value.trim();
    var password = document.getElementById('password').value;

    if (!ssid) {
      showToast('请输入WiFi名称','error');
      return;
    }

    var btn = document.getElementById('connectBtn');
    btn.textContent = '连接中...';
    btn.disabled = true;

    fetch('/api/connect', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ssid: ssid, password: password})
    })
    .then(function(r){ return r.json(); })
    .then(function(data) {
      btn.textContent = '连接';
      btn.disabled = false;
      if (data.success) {
        connected = true;
        currentSSID = ssid;
        updateConnectionUI();
        showToast('连接成功！', 'success');
        document.getElementById('statusDetail').textContent =
          'IP: ' + (data.ip || '--') + ' | 等待获取IP...';
        // 延迟刷新以获取IP
        setTimeout(refreshStatus, 2000);
      } else {
        showToast('连接失败: ' + (data.error || '未知错误'), 'error');
        updateConnectionUI();
      }
    })
    .catch(function() {
      btn.textContent = '连接';
      btn.disabled = false;
      showToast('请求失败','error');
    });
  }

  // 断开WiFi
  function disconnectWiFi() {
    fetch('/api/disconnect')
      .then(function(r){ return r.json(); })
      .then(function(data) {
        if (data.success) {
          connected = false;
          currentSSID = '';
          updateConnectionUI();
          showToast('已断开连接', 'info');
        }
      })
      .catch(function(){ showToast('操作失败','error'); });
  }

  // 页面加载
  document.addEventListener('DOMContentLoaded', function() {
    refreshStatus();
    // 自动扫描
    setTimeout(scanWiFi, 1000);
  });
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=utf-8", html);
}

void handle_scan()
{
  Serial.println("[Web] Scanning WiFi...");
  int n = WiFi.scanNetworks(false, true, false, 500);

  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURE";
    String ssid = WiFi.SSID(i);
    ssid.replace("\"", "\\\"");
    json += "{\"ssid\":\"" + ssid + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"ch\":" + String(WiFi.channel(i)) + ",";
    json += "\"enc\":\"" + enc + "\"}";
  }
  json += "]";

  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handle_connect()
{
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  // 简易JSON解析
  String body = server.arg("plain");
  String ssid = "";
  String password = "";

  int ssidStart = body.indexOf("\"ssid\":\"");
  if (ssidStart >= 0) {
    ssidStart += 8;
    int ssidEnd = body.indexOf("\"", ssidStart);
    if (ssidEnd > ssidStart) {
      ssid = body.substring(ssidStart, ssidEnd);
    }
  }

  int pwdStart = body.indexOf("\"password\":\"");
  if (pwdStart >= 0) {
    pwdStart += 12;
    int pwdEnd = body.indexOf("\"", pwdStart);
    if (pwdEnd > pwdStart) {
      password = body.substring(pwdStart, pwdEnd);
    }
  }

  if (ssid.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"SSID required\"}");
    return;
  }

  Serial.printf("[Web] Connect request: SSID=%s\n", ssid.c_str());

  bool ok = connect_to_wifi(ssid.c_str(), password.c_str());
  if (ok) {
    setup_nat();
    String resp = "{\"success\":true,\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    server.send(200, "application/json", resp);
  } else {
    server.send(200, "application/json", "{\"success\":false,\"error\":\"Connection failed\"}");
  }
}

void handle_status()
{
  String json = "{";
  json += "\"connected\":" + String(sta_connected ? "true" : "false") + ",";
  json += "\"ssid\":\"" + sta_ssid + "\",";
  json += "\"ip\":\"" + (sta_connected ? WiFi.localIP().toString() : "") + "\",";
  json += "\"rssi\":" + String(sta_connected ? WiFi.RSSI() : 0) + ",";
  json += "\"ap_ssid\":\"" AP_SSID "\",";
  json += "\"ap_clients\":" + String(WiFi.softAPgetStationNum());
  json += "}";
  server.send(200, "application/json", json);
}

void handle_disconnect()
{
  if (sta_connected) {
    WiFi.disconnect(false);
    sta_connected = false;
    sta_ssid = "";
    lv_label_set_text(sta_status_label, "STA: Disconnected");
    lv_obj_set_style_text_color(sta_status_label, lv_color_hex(0x888888), 0);
    lv_label_set_text(sta_ssid_label, "");
    lv_label_set_text(rate_label, "Rate: -- Mbps");
    lv_label_set_text(ip_label, "IP: --");
    Serial.println("[WiFi] Disconnected");

    // 恢复DNS劫持，重新进入配网模式
    configure_ap_dhcp_dns(WiFi.softAPIP().toString().c_str());
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.println("[DNS] Captive portal restarted");
  }
  server.send(200, "application/json", "{\"success\":true}");
}

// ===================== Setup =====================
void setup()
{
  delay(500);
  Serial.begin(115200);
  Serial.println("\n==============================");
  Serial.println("  ESP32-C5 WiFi Repeater");
  Serial.println("==============================");

  // LCD 初始化
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, 0);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ST7735_BLACK);

  // LVGL 初始化
  lv_init();
  lvgl_init();
  create_ui();

  // 读取保存的WiFi凭证
  prefs.begin("wifi-repeater", false);
  saved_ssid = prefs.getString("sta_ssid", "");
  saved_password = prefs.getString("sta_pwd", "");

  // 启动AP模式
  start_ap();

  // DNS服务器 (Captive Portal)
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("[DNS] Captive portal started");

  // Web服务器路由
  server.on("/", handle_root);
  server.on("/api/scan", handle_scan);
  server.on("/api/connect", HTTP_POST, handle_connect);
  server.on("/api/status", handle_status);
  server.on("/api/disconnect", handle_disconnect);
  server.onNotFound(handle_root); // 所有未匹配路由重定向到配置页
  server.begin();
  Serial.println("[Web] Server started on port " + String(WEB_PORT));

  // 自动连接已保存的WiFi
  if (saved_ssid.length() > 0) {
    Serial.println("[WiFi] Auto-connecting to saved network: " + saved_ssid);
    connect_to_wifi(saved_ssid.c_str(), saved_password.c_str());
    if (sta_connected) {
      setup_nat();
    }
  }

  Serial.println("\n[Ready] Connect to AP: " AP_SSID);
  Serial.println("[Ready] Open browser: http://" + apIP.toString());
  Serial.println("[Ready] Password: " AP_PASSWORD "\n");
}

// ===================== Loop =====================
void loop()
{
  dnsServer.processNextRequest();
  server.handleClient();

  // 定期更新LCD显示
  if (millis() - last_status_update > 2000) {
    last_status_update = millis();
    if (sta_connected) {
      update_display();
    }
  }

  // 处理WiFi重连
  if (sta_connected && WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost, reconnecting...");
    sta_connected = false;
    lv_label_set_text(sta_status_label, "STA: Reconnecting...");
    lv_obj_set_style_text_color(sta_status_label, lv_color_hex(0xffaa00), 0);
    WiFi.reconnect();
  }

  delay(10);
}
