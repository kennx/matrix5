#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include <cstring>

#include "ble_config_service.h"
#include "ble_display_text.h"
#include "config_json.h"
#include "config_store.h"
#include "config_types.h"
#include "config_validator.h"
#include "net_apply.h"
#include "pairing_code.h"
#include "paired_device_store.h"

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
PairingCodeManager pairingCodeMgr;
BleConfigService bleConfigService;

PairedDeviceStore pairedDeviceStore;
std::string currentConnectedAddr;
bool isPairedDevice = false;

DeviceConfig pendingConfig{};
bool hasPendingConfig = false;
bool isAuthorized = false;
bool bleStarted = false;
bool bleModeActive = false;
bool applyRequested = false;

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

static void drawBleScreen(const std::string& code) {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString("BLE CONFIG", M5.Display.width() / 2, 28);
    M5.Display.setTextSize(2);
    M5.Display.drawString(formatPairingCodeLine(code).c_str(), M5.Display.width() / 2, 64);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Use Chrome page to connect", M5.Display.width() / 2, 96);
    M5.Display.drawString("Hold A+B 5s to refresh", M5.Display.width() / 2, 112);
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

static void drawPairedConfirmScreen() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString("PAIRED", M5.Display.width() / 2, 28);
    M5.Display.setTextSize(2);
    M5.Display.drawString("Press BtnA", M5.Display.width() / 2, 64);
    M5.Display.setTextSize(1);
    M5.Display.drawString("to confirm", M5.Display.width() / 2, 96);
    M5.Display.drawString("Hold A+B 5s to refresh", M5.Display.width() / 2, 112);
}

static void enterBleConfigMode() {
    if (!bleStarted) {
        BleConfigCallbacks callbacks;
        callbacks.onConnect = [&](const std::string& addr) {
            currentConnectedAddr = addr;
            if (pairedDeviceStore.isPaired(addr)) {
                isPairedDevice = true;
                isAuthorized = true;  // 已配对设备自动授权
                drawStatusScreen("PAIRED", "Ready for config");
                delay(300);
                bleConfigService.notifyStatus(ConfigState::PairedDeviceConnected, ConfigError::Ok, "auto_auth");
            } else {
                isPairedDevice = false;
                const std::string newCode = pairingCodeMgr.generate(millis(), 120000);
                drawBleScreen(newCode);
                delay(300);
                bleConfigService.notifyStatus(ConfigState::BleAdvertising, ConfigError::Ok, newCode.c_str());
            }
        };

        callbacks.onDisconnect = [&]() {
            currentConnectedAddr.clear();
            isPairedDevice = false;
            isAuthorized = false;
            hasPendingConfig = false;
            applyRequested = false;
        };

        callbacks.onAuth = [&](const std::string& code) {
            ConfigError err = pairingCodeMgr.verify(code, millis());
            if (err == ConfigError::Ok) {
                isAuthorized = true;
                if (!currentConnectedAddr.empty() && !pairedDeviceStore.isPaired(currentConnectedAddr)) {
                    pairedDeviceStore.add(currentConnectedAddr);
                    prefs.putString("paired_devices", pairedDeviceStore.serialize().c_str());
                }
            }
            return err;
        };
        callbacks.onConfigJson = [&](const std::string& json) {
            if (!isAuthorized) return ConfigError::Unauthorized;
            DeviceConfig candidate;
            if (!parseConfigJson(json, candidate)) {
                return ConfigError::JsonParseFailed;
            }
            ConfigError valid = validateConfig(candidate);
            if (valid != ConfigError::Ok) {
                return valid;
            }
            pendingConfig = candidate;
            hasPendingConfig = true;
            return ConfigError::Ok;
        };
        callbacks.onCommand = [&](const std::string& cmd) {
            if (!isAuthorized) return ConfigError::Unauthorized;
            if (cmd != "apply" || !hasPendingConfig) return ConfigError::InvalidField;
            applyRequested = true;
            return ConfigError::Ok;
        };
        bleConfigService.begin(callbacks);
        bleStarted = true;
    }

    isAuthorized = false;
    hasPendingConfig = false;
    bleModeActive = true;
    isPairedDevice = false;
    currentConnectedAddr.clear();

    if (pairedDeviceStore.count() > 0) {
        drawStatusScreen("PAIRED", "Waiting for connect");
        bleConfigService.notifyStatus(ConfigState::PairedDeviceConnected, ConfigError::Ok, "waiting_connect");
    } else {
        const std::string newCode = pairingCodeMgr.generate(millis(), 120000);
        drawBleScreen(newCode);
        bleConfigService.notifyStatus(ConfigState::BleAdvertising, ConfigError::Ok, newCode.c_str());
    }
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
    pairedDeviceStore.deserialize(prefs.getString("paired_devices", "").c_str());

    DeviceConfig cfgData;
    const bool hasConfig = configStore.load(cfgData);
    if (hasConfig && validateConfig(cfgData) == ConfigError::Ok && applyNetworkConfig(cfgData) == ConfigError::Ok) {
        Serial.println("WiFi/NTP ready");
        bleModeActive = false;
    } else {
        enterBleConfigMode();
    }
}

void loop() {
    static unsigned long lastUpdate = 0;
    if (!bleModeActive && millis() - lastUpdate >= 1000) {
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

    M5.update();
    static bool reconfigTriggered = false;
    if (M5.BtnA.pressedFor(5000) && M5.BtnB.pressedFor(5000)) {
        if (!reconfigTriggered) {
            reconfigTriggered = true;
            enterBleConfigMode();
        }
    } else {
        reconfigTriggered = false;
    }

    // 长按 BtnB 5 秒清除所有配对记录
    static bool clearPairedTriggered = false;
    if (M5.BtnB.pressedFor(5000)) {
        if (!clearPairedTriggered) {
            clearPairedTriggered = true;
            pairedDeviceStore.clear();
            prefs.putString("paired_devices", "");
            drawStatusScreen("CLEARED", "All paired devices removed");
            delay(2000);
            enterBleConfigMode();
        }
    } else {
        clearPairedTriggered = false;
    }

    if (applyRequested) {
        applyRequested = false;
        if (!configStore.save(pendingConfig)) {
            bleConfigService.notifyStatus(ConfigState::Error, ConfigError::InvalidField, "save_failed");
        } else {
            ConfigError applied = applyNetworkConfig(pendingConfig);
            if (applied == ConfigError::Ok) {
                bleModeActive = false;
                bleConfigService.notifyStatus(ConfigState::Done, ConfigError::Ok, "applied");
                bleConfigService.stop();
                bleStarted = false;
                struct tm t;
                char timebuf[32];
                if (getLocalTime(&t)) {
                    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &t);
                } else {
                    std::strncpy(timebuf, "--:--:--", sizeof(timebuf));
                }
                drawStatusScreen("SYNC OK", timebuf);
            } else {
                bleConfigService.notifyStatus(ConfigState::Error, applied, "apply_failed");
            }
        }
    }

    M5.delay(10);
}
