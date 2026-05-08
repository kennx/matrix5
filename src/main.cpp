#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

// ========== LED 点阵字体 ==========
// 5×7 点阵，每行低 5 位有效（bit4=左, bit0=右）
static const uint8_t FONT[36][7] = {
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
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
};

static const uint8_t COLON[4] = {
    0x00, // 相对行 0: 空
    0x01, // 相对行 1: 点亮
    0x00, // 相对行 2: 空
    0x01, // 相对行 3: 点亮
};

static inline int charIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

// ========== LED 点阵背景参数 ==========
static const uint8_t DOT_SIZE    = 5;   // 每个大像素 5×5
static const uint8_t DOT_RADIUS  = 2;   // 圆半径
static const uint8_t DOT_BRIGHT  = 20;  // 255 * 0.08 ≈ 20

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
M5Canvas sprite(&M5.Display);
String wifiOptions;

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

    return WiFi.status() == WL_CONNECTED;
}

static void drawDot(int16_t col, int16_t row, uint16_t color) {
    int cx = col * DOT_SIZE + DOT_SIZE / 2;
    int cy = row * DOT_SIZE + DOT_SIZE / 2;
    sprite.fillCircle(cx, cy, DOT_RADIUS, color);
}

static void drawBackgroundDots() {
    int w = sprite.width();
    int h = sprite.height();
    int cols = w / DOT_SIZE;
    int rows = h / DOT_SIZE;
    uint16_t dimColor = sprite.color565(DOT_BRIGHT, DOT_BRIGHT, DOT_BRIGHT);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            drawDot(col, row, dimColor);
        }
    }
}

static void drawChar(int16_t startCol, int16_t startRow, const uint8_t mask[7]) {
    for (int row = 0; row < 7; row++) {
        uint8_t bits = mask[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                drawDot(startCol + col, startRow + row, WHITE);
            }
        }
    }
}

static void drawColon(int16_t startCol, int16_t startRow) {
    for (int row = 0; row < 4; row++) {
        if (COLON[row]) {
            drawDot(startCol, startRow + row, WHITE);
        }
    }
}

static void drawClock(const char* timeStr) {
    sprite.fillSprite(BLACK);
    drawBackgroundDots();

    int startCol = 3;   // (48 - 42) / 2 = 3
    int startRow = 10;  // (27 - 7) / 2 = 10
    int col = startCol;

    for (const char* p = timeStr; *p; p++) {
        if (*p == ':') {
            drawColon(col, startRow + 1);  // 冒号垂直居中偏移 1 行
            col += 1;
        } else {
            int idx = charIndex(*p);
            if (idx >= 0) {
                drawChar(col, startRow, FONT[idx]);
            }
            col += 5;
        }
    }

    sprite.pushSprite(0, 0);
}

// ========== 主程序 ==========
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);

    sprite.createSprite(M5.Display.width(), M5.Display.height());
    drawClock("00:00:00");

    prefs.begin("m5stick", false);

    String ssid = prefs.getString("ssid", "");
    Serial.printf("Saved SSID: %s\n", ssid.c_str());

    if (!tryConnectWiFi()) {
        Serial.println("WiFi failed, starting AP");
        startAPMode();
    } else {
        Serial.println("WiFi connected");
    }
}

void loop() {
    if (apMode) {
        server.handleClient();
    } else {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate >= 1000) {
            lastUpdate = millis();

            struct tm timeinfo;
            char buf[9];
            if (getLocalTime(&timeinfo)) {
                strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
            } else {
                strncpy(buf, "00:00:00", sizeof(buf));
            }

            drawClock(buf);
        }
    }
    M5.delay(10);
}
