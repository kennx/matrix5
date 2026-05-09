#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include <cstring>

#include "ble_config_service.h"
#include "config_store.h"
#include "config_types.h"
#include "config_validator.h"
#include "net_apply.h"
#include "pairing_code.h"

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

DeviceConfig pendingConfig{};
bool hasPendingConfig = false;
bool isAuthorized = false;
bool bleStarted = false;

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

static void enterBleConfigMode() {
    if (!bleStarted) {
        BleConfigCallbacks callbacks;
        callbacks.onAuth = [&](const std::string& code) {
            ConfigError err = pairingCodeMgr.verify(code, millis());
            isAuthorized = (err == ConfigError::Ok);
            return err;
        };
        callbacks.onConfigJson = [&](const std::string& json) {
            (void)json;
            if (!isAuthorized) return ConfigError::Unauthorized;
            return ConfigError::JsonParseFailed;
        };
        callbacks.onCommand = [&](const std::string& cmd) {
            if (!isAuthorized) return ConfigError::Unauthorized;
            if (cmd != "apply" || !hasPendingConfig) return ConfigError::InvalidField;
            if (!configStore.save(pendingConfig)) return ConfigError::InvalidField;
            return applyNetworkConfig(pendingConfig);
        };
        bleConfigService.begin(callbacks);
        bleStarted = true;
    }

    const std::string newCode = pairingCodeMgr.generate(millis(), 120000);
    isAuthorized = false;
    hasPendingConfig = false;
    bleConfigService.notifyStatus(ConfigState::BleAdvertising, ConfigError::Ok, newCode.c_str());
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
    if (hasConfig && validateConfig(cfgData) == ConfigError::Ok && applyNetworkConfig(cfgData) == ConfigError::Ok) {
        Serial.println("WiFi/NTP ready");
    } else {
        enterBleConfigMode();
    }
}

void loop() {
    static unsigned long lastUpdate = 0;
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

    M5.update();
    if (M5.BtnA.pressedFor(1500)) {
        enterBleConfigMode();
    }
    M5.delay(10);
}
