#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include <cstring>
#include <vector>
#include <algorithm>

#include "config_json.h"
#include "config_store.h"
#include "config_types.h"
#include "config_validator.h"
#include "net_apply.h"
#include "usb_config_service.h"

// ========== LED 点阵字体 ==========
static const uint8_t FONT[36][7] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x0E, 0x11, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}, {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
};

static const uint8_t COLON[4] = {0x00, 0x01, 0x00, 0x01};

static const uint8_t DOT_SIZE = 5;
static const uint8_t DOT_RADIUS = 2;
static const uint8_t DOT_BRIGHT = 20;

Preferences prefs;
ConfigStore configStore(prefs);
UsbConfigService usbConfigService;

bool configModeActive = false;
bool hasPendingConfig = false;
DeviceConfig pendingConfig{};
bool wifiScanRequested = false;
bool wifiScanInProgress = false;

M5Canvas sprite(&M5.Display);

static inline int charIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

static void drawDot(int16_t col, int16_t row, uint16_t color) {
    int cx = col * DOT_SIZE + DOT_SIZE / 2;
    int cy = row * DOT_SIZE + DOT_SIZE / 2;
    sprite.fillCircle(cx, cy, DOT_RADIUS, color);
}

static void drawBackgroundDots() {
    int cols = sprite.width() / DOT_SIZE;
    int rows = sprite.height() / DOT_SIZE;
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

    int startCol = 5;
    int startRow = 10;
    int col = startCol;

    for (const char* p = timeStr; *p; p++) {
        if (*p == ':') {
            drawColon(col, startRow + 1);
            col += 1;
        } else {
            int idx = charIndex(*p);
            if (idx >= 0) {
                drawChar(col, startRow, FONT[idx]);
            }
            col += 5;
        }
        if (*(p + 1) != '\0') {
            col += 1;
        }
    }

    sprite.pushSprite(0, 0);
}

static void drawConfigScreen() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString("USB CONFIG", M5.Display.width() / 2, M5.Display.height() / 2);
}

static void drawStatusScreen(const char* title, const char* line) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString(title, M5.Display.width() / 2, 40);
    M5.Display.setTextSize(1);
    M5.Display.drawString(line, M5.Display.width() / 2, 80);
}

static std::string escapeJsonString(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string buildScanResultJson(int scanCount) {
    struct Network { std::string ssid; int rssi; };
    std::vector<Network> networks;

    for (int i = 0; i < scanCount; i++) {
        std::string ssid = WiFi.SSID(i).c_str();
        int rssi = WiFi.RSSI(i);
        if (ssid.empty()) continue;
        auto it = std::find_if(networks.begin(), networks.end(),
            [&](const Network& n) { return n.ssid == ssid; });
        if (it != networks.end()) {
            if (rssi > it->rssi) it->rssi = rssi;
        } else {
            networks.push_back({ssid, rssi});
        }
    }

    std::sort(networks.begin(), networks.end(),
        [](const Network& a, const Network& b) { return a.rssi > b.rssi; });

    if (networks.size() > 5) networks.resize(5);

    std::string json = "[";
    for (size_t i = 0; i < networks.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + escapeJsonString(networks[i].ssid) + "\",\"rssi\":"
              + std::to_string(networks[i].rssi) + "}";
    }
    json += "]";
    return json;
}

static void enterConfigMode() {
    configModeActive = true;
    drawConfigScreen();

    UsbConfigCallbacks callbacks;
    callbacks.onApply = [&](const DeviceConfig& cfg) {
        pendingConfig = cfg;
        hasPendingConfig = true;
    };
    callbacks.onScanWifi = [&]() {
        wifiScanRequested = true;
    };
    callbacks.onGetConfig = [&]() {
        DeviceConfig cfg;
        if (configStore.load(cfg)) {
            usbConfigService.sendConfig(&cfg);
        } else {
            usbConfigService.sendConfig(nullptr);
        }
    };
    usbConfigService.begin(callbacks);
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);

    sprite.createSprite(M5.Display.width(), M5.Display.height());
    drawClock("00:00:00");

    prefs.begin("m5stick", false);

    DeviceConfig cfgData;
    const bool hasConfig = configStore.load(cfgData);
    if (hasConfig && validateConfig(cfgData) == ConfigError::Ok) {
        M5.Display.setBrightness(cfgData.brightness * 255 / 100);
        if (!cfgData.wifiSsid.empty()) {
            applyNetworkConfig(cfgData);
        }
        configModeActive = false;
    } else {
        M5.Display.setBrightness(50 * 255 / 100);
        enterConfigMode();
    }
}

void loop() {
    static unsigned long lastUpdate = 0;

    if (configModeActive) {
        usbConfigService.loop();

        if (hasPendingConfig) {
            hasPendingConfig = false;
            ConfigError valid = validateConfig(pendingConfig);
            if (valid != ConfigError::Ok) {
                usbConfigService.sendError(valid, "validation_failed");
            } else if (!configStore.save(pendingConfig)) {
                usbConfigService.sendError(ConfigError::InvalidField, "save_failed");
            } else {
                M5.Display.setBrightness(pendingConfig.brightness * 255 / 100);

                uint32_t ts = usbConfigService.getPendingTime();
                if (ts > 0) {
                    timeval tv = { static_cast<time_t>(ts), 0 };
                    settimeofday(&tv, nullptr);
                }

                if (!pendingConfig.wifiSsid.empty()) {
                    applyNetworkConfig(pendingConfig);
                }

                usbConfigService.sendOk("applied");
                configModeActive = false;
            }
        }

        if (wifiScanRequested && !wifiScanInProgress) {
            wifiScanRequested = false;
            wifiScanInProgress = true;
            WiFi.scanNetworks(true);
        }

        if (wifiScanInProgress) {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                wifiScanInProgress = false;
                if (n == 0) {
                    usbConfigService.sendScanResult("{\"type\":\"scan_result\",\"networks\":[]}");
                } else {
                    std::string json = "{\"type\":\"scan_result\",\"networks\":";
                    json += buildScanResultJson(n);
                    json += "}";
                    usbConfigService.sendScanResult(json);
                }
                WiFi.scanDelete();
            } else if (n == -2) {
                wifiScanInProgress = false;
                usbConfigService.sendError(ConfigError::WifiScanFailed, "scan_failed");
            }
        }
    } else {
        if (millis() - lastUpdate >= 1000) {
            lastUpdate = millis();

            struct tm timeinfo;
            char buf[9];
            if (getLocalTime(&timeinfo)) {
                strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
            } else {
                std::strncpy(buf, "00:00:00", sizeof(buf));
            }
            drawClock(buf);
        }
    }

    M5.update();
    static bool reconfigTriggered = false;
    if (M5.BtnA.pressedFor(5000) && M5.BtnB.pressedFor(5000)) {
        if (!reconfigTriggered) {
            reconfigTriggered = true;
            enterConfigMode();
        }
    } else {
        reconfigTriggered = false;
    }

    M5.delay(10);
}
