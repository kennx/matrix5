#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

// ========== LED 点阵字体 ==========
static const char* DEFAULT_TIME = "12:34:56";

static const uint8_t LED_MASKS[11][7] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x0E, 0x11, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E}, // 9
    {0x00, 0x00, 0x04, 0x00, 0x00, 0x04, 0x00}, // : 单像素居中
};

static const uint8_t DOT_PITCH = 4;   // 相邻圆点中心距离
static const uint8_t COLS = 5;
static const uint8_t ROWS = 7;
static const uint8_t CHAR_GAP = 2;    // 字符之间额外间隙

static inline int ledIndex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch == ':' || ch == '.') return 10;
    return -1;
}

template <typename T>
static void drawLEDCharGfx(T* gfx, int16_t x, int16_t y, char ch, uint16_t color) {
    int idx = ledIndex(ch);
    if (idx < 0) return;

    const uint8_t* mask = LED_MASKS[idx];
    for (int row = 0; row < ROWS; row++) {
        uint8_t bits = mask[row];
        for (int col = 0; col < COLS; col++) {
            if (bits & (1 << (4 - col))) {
                int cx = x + col * DOT_PITCH + DOT_PITCH / 2;
                int cy = y + row * DOT_PITCH + DOT_PITCH / 2;
                gfx->drawPixel(cx, cy, color);
            }
        }
    }
}

template <typename T>
static void drawLEDStringGfx(T* gfx, int16_t x, int16_t y, const char* str, uint16_t color) {
    while (*str) {
        drawLEDCharGfx(gfx, x, y, *str, color);
        x += COLS * DOT_PITCH + CHAR_GAP;
        str++;
    }
}

static void drawLEDChar(int16_t x, int16_t y, char ch, uint16_t color) {
    drawLEDCharGfx(&M5.Display, x, y, ch, color);
}

static void drawLEDString(int16_t x, int16_t y, const char* str, uint16_t color) {
    drawLEDStringGfx(&M5.Display, x, y, str, color);
}

// ========== WiFi 配置页面 ==========
static const char* CONFIG_PAGE_HEAD =
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>M5StickS3</title>"
"<style>*{margin:0;padding:0;box-sizing:border-box}"
"body{background:#000;color:#fff;font-family:sans-serif;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:20px}"
".container{width:100%%;max-width:320px}"
"h1{text-align:center;margin-bottom:20px;font-size:20px;letter-spacing:2px}"
".input-group{margin-bottom:15px}"
"label{display:block;margin-bottom:8px;font-size:14px;color:#aaa}"
"select{width:100%%;padding:12px;border:1px solid #333;border-radius:8px;background:#111;color:#fff;font-size:16px;cursor:pointer}"
"select:focus{outline:none;border-color:#fff}"
"input{width:100%%;padding:12px;border:1px solid #333;border-radius:8px;background:#111;color:#fff;font-size:16px}"
"input:focus{outline:none;border-color:#fff}"
"button{width:100%%;padding:14px;margin-top:10px;border:none;border-radius:8px;background:#fff;color:#000;font-size:16px;font-weight:bold;cursor:pointer}"
".info{text-align:center;margin-top:20px;font-size:12px;color:#666;line-height:1.8}"
"</style></head><body>"
"<div class=\"container\"><h1>M5StickS3</h1>"
"<form action=\"/save\" method=\"POST\">"
"<div class=\"input-group\"><label>WiFi 网络</label><select id=\"wifi-select\" onchange=\"document.getElementById('ssid').value=this.value\">"
"<option value=\"\">选择 WiFi 网络...</option>";

static const char* CONFIG_PAGE_TAIL =
"</select></div>"
"<div class=\"input-group\"><label>SSID</label><input type=\"text\" name=\"ssid\" id=\"ssid\" placeholder=\"WiFi 名称或手动输入\" required></div>"
"<div class=\"input-group\"><label>密码</label><input type=\"password\" name=\"password\" placeholder=\"WiFi 密码\"></div>"
"<div class=\"input-group\"><label>时区</label><select name=\"timezone\">"
"<option value=\"0\">UTC 格林尼治</option>"
"<option value=\"3600\">UTC+1 柏林/巴黎</option>"
"<option value=\"7200\">UTC+2 开罗</option>"
"<option value=\"19800\">UTC+5:30 新德里</option>"
"<option value=\"25200\">UTC+7 曼谷</option>"
"<option value=\"28800\" selected>UTC+8 北京/上海</option>"
"<option value=\"32400\">UTC+9 东京/首尔</option>"
"<option value=\"36000\">UTC+10 悉尼</option>"
"<option value=\"-18000\">UTC-5 纽约</option>"
"<option value=\"-25200\">UTC-7 丹佛</option>"
"<option value=\"-28800\">UTC-8 洛杉矶</option>"
"</select></div>"
"<button type=\"submit\">保存</button></form>"
"<div class=\"info\">SSID: M5Stick-Setup<br>IP: 192.168.4.1</div></div>"
"</body></html>";

static const char* SAVE_OK_PAGE =
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
"<title>OK</title><style>body{background:#000;color:#fff;text-align:center;padding-top:100px}"
"h2{margin-bottom:20px}</style></head>"
"<body><h2>保存成功</h2><p>重启中...</p></body></html>";

// ========== WiFi 配置逻辑 ==========
Preferences prefs;
WebServer server(80);
bool apMode = false;
String wifiOptions;
M5Canvas timeCanvas(&M5.Display);
static bool canvasCreated = false;

static String escapeHtml(const String& s) {
    String r = s;
    r.replace("&", "&amp;");
    r.replace("\"", "&quot;");
    r.replace("<", "&lt;");
    r.replace(">", "&gt;");
    return r;
}

void scanWiFiNetworks() {
    wifiOptions = "";
    int n = WiFi.scanNetworks();
    Serial.printf("WiFi scan found %d networks\n", n);
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        String safeSsid = escapeHtml(ssid);
        wifiOptions += "<option value=\"" + safeSsid + "\">" + safeSsid + " (" + String(rssi) + "dBm)</option>";
    }
    if (n == 0) {
        wifiOptions = "<option value=\"\">未找到 WiFi 网络</option>";
    }
}

void handleRoot() {
    String page = String(CONFIG_PAGE_HEAD) + wifiOptions + String(CONFIG_PAGE_TAIL);
    server.send(200, "text/html", page);
}

void handleSave() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String tzStr = server.arg("timezone");

    if (ssid.length() > 0) {
        prefs.putString("ssid", ssid);
        prefs.putString("password", password);
        prefs.putLong("tz_offset", tzStr.toInt());
        server.send(200, "text/html", SAVE_OK_PAGE);
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "WiFi 名称不能为空");
    }
}

void startAPMode() {
    apMode = true;
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);

    IPAddress localIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(localIP, gateway, subnet);

    bool ok = WiFi.softAP("M5Stick-Setup");
    Serial.printf("AP start: %s\n", ok ? "OK" : "FAIL");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    scanWiFiNetworks();

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.println("WebServer started");

    M5.Display.fillScreen(BLACK);

    int16_t charW = COLS * DOT_PITCH;
    int16_t charH = ROWS * DOT_PITCH;
    int16_t gap = 8;
    int16_t line1Len = 7; // "192.168"
    int16_t line2Len = 3; // "4.1"
    int16_t line1W = line1Len * charW + (line1Len - 1) * CHAR_GAP;
    int16_t line2W = line2Len * charW + (line2Len - 1) * CHAR_GAP;
    int16_t totalH = charH * 2 + gap;
    int16_t y1 = (M5.Display.height() - totalH) / 2;
    int16_t y2 = y1 + charH + gap;
    int16_t x1 = (M5.Display.width() - line1W) / 2;
    int16_t x2 = (M5.Display.width() - line2W) / 2;

    drawLEDString(x1, y1, "192.168", WHITE);
    drawLEDString(x2, y2, "4.1", WHITE);
}

bool tryConnectWiFi() {
    String ssid = prefs.getString("ssid", "");
    String password = prefs.getString("password", "");

    if (ssid.length() == 0) return false;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        long tzOffset = prefs.getLong("tz_offset", 28800);
        configTime(tzOffset, 0, "pool.ntp.org", "cn.pool.ntp.org");
        Serial.printf("NTP sync started, TZ offset: %ld\n", tzOffset);
        return true;
    }
    return false;
}

void showTime() {
    struct tm timeinfo;
    char buf[9];
    if (getLocalTime(&timeinfo)) {
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    } else {
        strncpy(buf, DEFAULT_TIME, sizeof(buf));
        buf[sizeof(buf) - 1] = '\0';
    }

    int16_t charW = COLS * DOT_PITCH;
    int16_t charH = ROWS * DOT_PITCH;
    int16_t len = strlen(buf);
    int16_t textW = len * charW + (len - 1) * CHAR_GAP;
    int16_t textH = charH;
    int16_t screenX = (M5.Display.width() - textW) / 2;
    int16_t screenY = (M5.Display.height() - textH) / 2;

    if (!canvasCreated) {
        timeCanvas.createSprite(textW, textH);
        canvasCreated = true;
    }

    timeCanvas.fillScreen(BLACK);
    drawLEDStringGfx(&timeCanvas, 0, 0, buf, WHITE);
    timeCanvas.pushSprite(screenX, screenY);
}

// ========== 主程序 ==========
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);

    prefs.begin("m5stick", false);

    String ssid = prefs.getString("ssid", "");
    Serial.printf("Saved SSID: %s\n", ssid.c_str());

    if (tryConnectWiFi()) {
        Serial.println("WiFi connected");
        showTime();
    } else {
        Serial.println("WiFi failed, starting AP");
        startAPMode();
    }
}

void loop() {
    if (apMode) {
        server.handleClient();
    } else {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate >= 1000) {
            lastUpdate = millis();
            showTime();
        }
    }
    M5.delay(10);
}
