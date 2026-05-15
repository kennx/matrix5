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
#include "battery_estimator.h"
#include "net_apply.h"
#include "time_sync_utils.h"
#include "usb_config_service.h"
#include "pomodoro.h"
#include "display_logic.h"
#include "orientation.h"

#ifndef BATTERY_DEBUG
#define BATTERY_DEBUG 0
#endif

// ========== LED 点阵字体 ==========
static const uint8_t FONT[37][7] = {
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
    {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
};

static const uint8_t COLON[4] = {0x00, 0x01, 0x00, 0x01};

static const uint8_t DOT_SIZE = 5;
static const uint8_t DOT_RADIUS = 2;
static const uint8_t DOT_BRIGHT = 20;

static uint8_t toBacklightLevel(uint8_t percent) {
    return static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255 + 50) / 100);
}

Preferences prefs;
ConfigStore configStore(prefs);
UsbConfigService usbConfigService;
BatteryEstimator batteryEstimator;
BatteryEstimate lastBatteryEstimate{0, 0.0f, 0.0f, false, false};
bool hasBatteryEstimate = false;

bool configModeActive = false;
bool hasPendingConfig = false;
DeviceConfig pendingConfig{};
bool wifiScanRequested = false;
bool wifiScanInProgress = false;

M5Canvas sprite(&M5.Display);
Pomodoro pomodoro;
OrientationManager orientationMgr;
bool forceOrientationRedraw = false;

enum class DisplayMode {
    Clock,
    Date,
    Battery,
    Count,
};

static DisplayMode displayMode = DisplayMode::Clock;

static bool isBatteryCharging() {
    return M5.Power.isCharging() == m5::Power_Class::is_charging;
}

static void refreshBatteryEstimate(unsigned long nowMs) {
    static unsigned long lastBatterySample = 0;
    if (hasBatteryEstimate && nowMs - lastBatterySample < BatteryEstimator::SAMPLE_INTERVAL_MS) {
        return;
    }

    BatterySample sample{M5.Power.getBatteryVoltage(), isBatteryCharging(), nowMs};
    lastBatteryEstimate = batteryEstimator.update(sample);
    hasBatteryEstimate = true;
    lastBatterySample = nowMs;

    if (batteryEstimator.profileDirty()) {
        if (configStore.saveBatteryProfile(batteryEstimator.profile())) {
            batteryEstimator.clearProfileDirty();
        }
    }

#if BATTERY_DEBUG
    Serial.printf(
        "battery mv=%d chg=%d base=%.1f runtime=%.1f display=%d samples=%u\n",
        sample.voltageMv,
        sample.charging ? 1 : 0,
        lastBatteryEstimate.baselinePercent,
        lastBatteryEstimate.runtimePercent,
        lastBatteryEstimate.displayPercent,
        batteryEstimator.profile().sampleCount);
#endif
}

static inline int charIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c == '%') return 36;
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

static void drawColonRotated(int16_t startCol, int16_t startRow) {
    drawDot(startCol + 1, startRow, WHITE);
    drawDot(startCol + 3, startRow, WHITE);
}

static int textWidthInCols(const char* text, int len) {
    if (len <= 0) {
        return 0;
    }
    return len * 5 + (len - 1);
}

static void drawTextLine(const char* text, int len, int startCol, int row) {
    int col = startCol;
    for (int i = 0; i < len; i++) {
        int idx = charIndex(text[i]);
        if (idx >= 0) {
            drawChar(col, row, FONT[idx]);
        }
        col += 5;
        if (i + 1 < len) {
            col += 1;
        }
    }
}

static void drawClock(const char* timeStr, ScreenOrientation orient) {
    sprite.fillSprite(BLACK);
    drawBackgroundDots();

    if (orient == ScreenOrientation::Portrait) {
        bool hasHours = (timeStr[2] == ':' && timeStr[5] == ':');
        int col = (sprite.width() / DOT_SIZE - 10) / 2;  // 2 个数字宽 10 列，居中
        int row = hasHours ? 8 : 12;  // 5 行或 3 行，调整顶部留白

        const char* p = timeStr;

        auto drawPair = [&](const char* digits) {
            int idx0 = charIndex(digits[0]);
            int idx1 = charIndex(digits[1]);
            if (idx0 >= 0) drawChar(col, row, FONT[idx0]);
            if (idx1 >= 0) drawChar(col + 6, row, FONT[idx1]);
            row += 9;  // 7 行高 + 2 行间距
        };

        if (hasHours) {
            drawPair(p);          // HH
            drawColonRotated(col + 3, row);
            row += 4;  // 1 行高 + 3 行间距
            p += 3;
        }
        drawPair(p);              // MM
        drawColonRotated(col + 3, row);
        row += 4;
        p += 3;
        drawPair(p);              // SS
    } else {
        // Landscape（现有逻辑，保持完全不变）
        int totalCols = 0;
        int len = 0;
        for (const char* p = timeStr; *p; p++) {
            totalCols += (*p == ':') ? 1 : 5;
            len++;
        }
        if (len > 1) totalCols += (len - 1);

        int startCol = (sprite.width() / DOT_SIZE - totalCols) / 2;
        if (startCol < 0) startCol = 0;
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
    }

    sprite.pushSprite(0, 0);
}

static void drawDate(const char* dateStr, ScreenOrientation orient) {
    sprite.fillSprite(BLACK);
    drawBackgroundDots();

    if (orient == ScreenOrientation::Portrait) {
        // 解析 "MON 05 13" 为三部分
        char part1[8] = {0}, part2[8] = {0}, part3[8] = {0};
        sscanf(dateStr, "%s %s %s", part1, part2, part3);

        const char* parts[3] = { part1, part2, part3 };
        int widths[3] = { 0, 0, 0 };

        // 计算每部分宽度（字符数 * 5 + 间距）
        for (int i = 0; i < 3; i++) {
            int len = 0;
            for (const char* p = parts[i]; *p; p++) {
                if (charIndex(*p) >= 0) {
                    widths[i] += 5;
                    len++;
                }
            }
            if (len > 1) widths[i] += (len - 1);
        }

        int maxWidth = widths[0];
        if (widths[1] > maxWidth) maxWidth = widths[1];
        if (widths[2] > maxWidth) maxWidth = widths[2];

        int colBase = (sprite.width() / DOT_SIZE - maxWidth) / 2;
        int lineHeight = 9;  // 7 行高 + 2 行间距
        int extraGap = 3;    // 周几和日期之间的额外间距
        int contentHeight = 3 * lineHeight - 2 + extraGap;
        int row = (sprite.height() / DOT_SIZE - contentHeight) / 2;

        for (int i = 0; i < 3; i++) {
            int col = colBase + (maxWidth - widths[i]) / 2;  // 每行独立居中
            for (const char* p = parts[i]; *p; p++) {
                int idx = charIndex(*p);
                if (idx >= 0) {
                    drawChar(col, row, FONT[idx]);
                }
                col += 5;
                if (*(p + 1) != '\0') {
                    col += 1;
                }
            }
            row += lineHeight;
            if (i == 0) row += extraGap;
        }
    } else {
        // Landscape（现有逻辑，保持完全不变）
        int startCol = 2;
        int startRow = 10;
        int col = startCol;

        for (const char* p = dateStr; *p; p++) {
            if (*p == ' ') {
                col += 2;
            } else {
                int idx = charIndex(*p);
                if (idx >= 0) {
                    drawChar(col, startRow, FONT[idx]);
                }
                col += 5;
            }
            if (*(p + 1) != '\0' && *p != ' ') {
                col += 1;
            }
        }
    }

    sprite.pushSprite(0, 0);
}

static void drawBattery(const char* batteryStr, ScreenOrientation orient) {
    sprite.fillSprite(BLACK);
    drawBackgroundDots();

    if (orient == ScreenOrientation::Portrait) {
        const char* pctPos = strchr(batteryStr, '%');
        if (pctPos != nullptr) {
            // 电量百分比：数字和 % 分行显示
            int numLen = static_cast<int>(pctPos - batteryStr);
            int numWidth = numLen * 5 + (numLen > 1 ? numLen - 1 : 0);
            int numCol = (sprite.width() / DOT_SIZE - numWidth) / 2;
            int pctCol = (sprite.width() / DOT_SIZE - 5) / 2;
            int contentHeight = 7 + 2 + 7;  // 数字高 + 间距 + % 高
            int row = (sprite.height() / DOT_SIZE - contentHeight) / 2;

            // 绘制数字
            int col = numCol;
            for (const char* p = batteryStr; p < pctPos; p++) {
                int idx = charIndex(*p);
                if (idx >= 0) drawChar(col, row, FONT[idx]);
                col += 5;
                if (p + 1 < pctPos) col += 1;
            }

            // 绘制 %
            row += 9;  // 7 高 + 2 间距
            int idx = charIndex('%');
            if (idx >= 0) drawChar(pctCol, row, FONT[idx]);
        } else if (strchr(batteryStr, ' ') != nullptr) {
            // 空格分隔（如 "25 MIN"）：空格前后分行显示
            const char* spacePos = strchr(batteryStr, ' ');
            int firstLen = static_cast<int>(spacePos - batteryStr);
            int secondLen = 0;
            for (const char* p = spacePos + 1; *p; p++) secondLen++;

            int firstWidth = textWidthInCols(batteryStr, firstLen);
            int secondWidth = textWidthInCols(spacePos + 1, secondLen);
            int firstCol = (sprite.width() / DOT_SIZE - firstWidth) / 2;
            int secondCol = (sprite.width() / DOT_SIZE - secondWidth) / 2;
            int contentHeight = 7 + 2 + 7;
            int row = (sprite.height() / DOT_SIZE - contentHeight) / 2;

            drawTextLine(batteryStr, firstLen, firstCol, row);
            row += 9;
            drawTextLine(spacePos + 1, secondLen, secondCol, row);
        } else {
            // 普通字符串：优先单行，超宽时分两行。
            int len = 0;
            for (const char* p = batteryStr; *p; p++) {
                len++;
            }

            int split = choosePortraitTextSplit(batteryStr, sprite.width() / DOT_SIZE);
            if (split > 0) {
                int firstWidth = textWidthInCols(batteryStr, split);
                int secondWidth = textWidthInCols(batteryStr + split, len - split);
                int row = (sprite.height() / DOT_SIZE - (7 + 2 + 7)) / 2;
                drawTextLine(batteryStr, split, (sprite.width() / DOT_SIZE - firstWidth) / 2, row);
                row += 9;
                drawTextLine(batteryStr + split, len - split, (sprite.width() / DOT_SIZE - secondWidth) / 2, row);
            } else {
                int totalCols = textWidthInCols(batteryStr, len);
                int startCol = (sprite.width() / DOT_SIZE - totalCols) / 2;
                if (startCol < 0) startCol = 0;
                int startRow = (sprite.height() / DOT_SIZE - 7) / 2;
                drawTextLine(batteryStr, len, startCol, startRow);
            }
        }
    } else {
        // Landscape：横向居中，空格用窄分隔
        int totalCols = 0;
        for (const char* p = batteryStr; *p; p++) {
            if (*p == ' ') {
                totalCols += 2;
            } else {
                totalCols += 5;
            }
            if (*(p + 1) != '\0') {
                totalCols += 1;
            }
        }

        int startCol = (sprite.width() / DOT_SIZE - totalCols) / 2;
        if (startCol < 0) startCol = 0;
        int startRow = 10;
        int col = startCol;

        for (const char* p = batteryStr; *p; p++) {
            if (*p == ' ') {
                col += 2;
            } else {
                int idx = charIndex(*p);
                if (idx >= 0) drawChar(col, startRow, FONT[idx]);
                col += 5;
            }
            if (*(p + 1) != '\0') {
                col += 1;
            }
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
    cfg.internal_imu = true;
    M5.begin(cfg);
    Serial.begin(115200);

    // 降低 CPU 频率到 80MHz (默认 240MHz)，节约 ~20mA 功耗，且不影响 SPI 屏幕刷新
    setCpuFrequencyMhz(80);

    if (M5.Imu.isEnabled()) {
        float ax, ay, az;
        M5.Imu.getAccel(&ax, &ay, &az);
        orientationMgr.begin(ax, ay);
    }

    ScreenOrientation initialOrient = orientationMgr.getOrientation();
    M5.Display.setRotation(initialOrient == ScreenOrientation::Landscape ? 1 : 0);
    M5.Display.fillScreen(BLACK);

    sprite.createSprite(M5.Display.width(), M5.Display.height());
    drawClock("00:00:00", initialOrient);

    prefs.begin("m5stick", false);
    batteryEstimator.begin();
    BatteryProfile storedBatteryProfile;
    if (configStore.loadBatteryProfile(storedBatteryProfile)) {
        batteryEstimator.loadProfile(storedBatteryProfile);
    }
    refreshBatteryEstimate(millis());

    DeviceConfig cfgData;
    const bool hasConfig = configStore.load(cfgData);
    if (hasConfig && validateConfig(cfgData) == ConfigError::Ok) {
        M5.Display.setBrightness(toBacklightLevel(cfgData.brightness));
        applyTimezoneEnv(cfgData.timezone);
        if (!cfgData.wifiSsid.empty()) {
            if (applyNetworkConfig(cfgData) != ConfigError::Ok) {
                enterConfigMode();
                return;
            }
        }
        configModeActive = false;
    } else {
        M5.Display.setBrightness(toBacklightLevel(50));
        enterConfigMode();
    }
}

void loop() {
    static unsigned long lastUpdate = 0;
    static bool suppressPomoBtnALongUntilRelease = false;

    M5.update();
    unsigned long nowMs = millis();

    // 新增：IMU 方向检测 (限制频率为 10Hz 节约 I2C 功耗)
    static unsigned long lastImuUpdate = 0;
    if (nowMs - lastImuUpdate >= 100) {
        lastImuUpdate = nowMs;
        float ax, ay, az;
        if (M5.Imu.isEnabled()) {
            M5.Imu.getAccel(&ax, &ay, &az);
            orientationMgr.update(ax, ay, nowMs);
        }
    }
    refreshBatteryEstimate(nowMs);

    // 新增：方向变化时重建 sprite
    static ScreenOrientation lastOrient = orientationMgr.getOrientation();
    ScreenOrientation currentOrient = orientationMgr.getOrientation();
    if (currentOrient != lastOrient) {
        lastOrient = currentOrient;
        int rotation = (currentOrient == ScreenOrientation::Landscape) ? 1 : 0;
        M5.Display.setRotation(rotation);
        sprite.deleteSprite();
        sprite.createSprite(M5.Display.width(), M5.Display.height());
        forceOrientationRedraw = true;
    }

    if (!M5.BtnA.pressedFor(1)) {
        suppressPomoBtnALongUntilRelease = false;
    }

    const bool bothButtonsPressed = M5.BtnA.pressedFor(1) && M5.BtnB.pressedFor(1);

    if (configModeActive) {
        if (forceOrientationRedraw) {
            drawConfigScreen();
            forceOrientationRedraw = false;
        }

        usbConfigService.loop();

        if (hasPendingConfig) {
            hasPendingConfig = false;
            ConfigError valid = validateConfig(pendingConfig);
            if (valid != ConfigError::Ok) {
                usbConfigService.sendError(valid, "validation_failed");
            } else if (!configStore.save(pendingConfig)) {
                usbConfigService.sendError(ConfigError::InvalidField, "save_failed");
            } else {
                M5.Display.setBrightness(toBacklightLevel(pendingConfig.brightness));
                applyTimezoneEnv(pendingConfig.timezone);

                uint32_t ts = usbConfigService.getPendingTime();
                if (ts > 0) {
                    timeval tv = { static_cast<time_t>(ts), 0 };
                    settimeofday(&tv, nullptr);
                }

                ConfigError netErr = ConfigError::Ok;
                if (!pendingConfig.wifiSsid.empty()) {
                    netErr = applyNetworkConfig(pendingConfig);
                }

                if (netErr == ConfigError::Ok) {
                    usbConfigService.sendOk("applied");
                    configModeActive = false;
                } else {
                    usbConfigService.sendError(netErr, "apply_failed");
                }
            }
        }

        if (wifiScanRequested && !wifiScanInProgress) {
            wifiScanRequested = false;
            wifiScanInProgress = true;
            WiFi.mode(WIFI_STA); // 确保 WiFi 模块已开启，否则扫描会失败
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
    } else if (pomodoro.isActive()) {
        // --- 番茄钟活跃 ---
        pomodoro.update(nowMs);
        const auto stateBeforeInput = pomodoro.getState();
        const auto phaseBeforeInput = pomodoro.getPhase();

        // 蜂鸣
        if (pomodoro.shouldBeep()) {
            M5.Speaker.tone(1000, 200);
            pomodoro.clearBeep();
        }

        // 按键分发
        if (M5.BtnA.wasClicked()) {
            pomodoro.onBtnAClick();
        }
        static bool pomoBtnALongTriggered = false;
        if (!bothButtonsPressed && !suppressPomoBtnALongUntilRelease && M5.BtnA.pressedFor(1000)) {
            if (!pomoBtnALongTriggered) {
                pomoBtnALongTriggered = true;
                pomodoro.onBtnALongPress();
            }
        } else {
            pomoBtnALongTriggered = false;
        }

        if (M5.BtnB.wasClicked()) {
            pomodoro.onBtnBClick();
        }
        static bool pomoBtnBLongTriggered = false;
        if (!bothButtonsPressed && M5.BtnB.pressedFor(1000)) {
            if (!pomoBtnBLongTriggered) {
                pomoBtnBLongTriggered = true;
                pomodoro.onBtnBLongPress();
            }
        } else {
            pomoBtnBLongTriggered = false;
        }

        const bool forcePomoRedraw =
            pomodoro.getState() != stateBeforeInput || pomodoro.getPhase() != phaseBeforeInput || forceOrientationRedraw;

        // 渲染
        static unsigned long lastPomoUpdate = 0;
        if (pomodoro.getState() == Pomodoro::State::ModeSelect) {
            // 模式选择页：每次都刷新（响应按键切换）
            char modeBuf[16];
            pomodoro.getModeWorkTimeDisplay(modeBuf, sizeof(modeBuf));
            drawBattery(modeBuf, orientationMgr.getOrientation());
        } else if (pomodoro.getState() == Pomodoro::State::Running) {
            // 计时页：每秒更新
            if (forcePomoRedraw || nowMs - lastPomoUpdate >= 1000) {
                lastPomoUpdate = nowMs;
                char buf[6];
                pomodoro.getTimeDisplay(buf, sizeof(buf));
                drawClock(buf, orientationMgr.getOrientation());
            }
        } else if (forcePomoRedraw) {
            // Paused：仅在状态/阶段变化时刷新一次
            char buf[6];
            pomodoro.getTimeDisplay(buf, sizeof(buf));
            drawClock(buf, orientationMgr.getOrientation());
        }
        forceOrientationRedraw = false;
    } else {
        // --- 原有时钟/日期/电量逻辑 ---
        if (M5.BtnA.wasClicked()) {
            displayMode = static_cast<DisplayMode>(
                (static_cast<int>(displayMode) + 1) % static_cast<int>(DisplayMode::Count));
        }

        // 长按 BtnA 1s 进入番茄钟
        static bool clockBtnALongTriggered = false;
        if (!bothButtonsPressed && M5.BtnA.pressedFor(1000)) {
            if (!clockBtnALongTriggered) {
                clockBtnALongTriggered = true;
                pomodoro.enter();
                suppressPomoBtnALongUntilRelease = true;
            }
        } else {
            clockBtnALongTriggered = false;
        }

        if (shouldRenderPeriodicFrame(nowMs, lastUpdate, 1000, forceOrientationRedraw)) {
            if (nowMs - lastUpdate >= 1000) {
                lastUpdate = nowMs;
            }

            struct tm timeinfo;
            if (displayMode == DisplayMode::Clock) {
                char buf[9];
                if (getLocalTime(&timeinfo)) {
                    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
                } else {
                    std::strncpy(buf, "00:00:00", sizeof(buf));
                }
                drawClock(buf, orientationMgr.getOrientation());
            } else if (displayMode == DisplayMode::Date) {
                char buf[16];
                if (!getLocalTime(&timeinfo) || strftime(buf, sizeof(buf), "%a %m %d", &timeinfo) == 0) {
                    std::strncpy(buf, "--- -- --", sizeof(buf));
                    buf[sizeof(buf) - 1] = '\0';
                }
                drawDate(buf, orientationMgr.getOrientation());
            } else if (displayMode == DisplayMode::Battery) {
                char buf[8];
                int batteryPercent = hasBatteryEstimate ? lastBatteryEstimate.displayPercent : 0;
                snprintf(buf, sizeof(buf), "%d%%", batteryPercent);
                drawBattery(buf, orientationMgr.getOrientation());
            }
            forceOrientationRedraw = false;
        }
    }

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
