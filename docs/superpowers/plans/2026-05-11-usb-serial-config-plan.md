# USB Serial 配置改造 + 亮度设置 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 推翻 BLE 配置方案，改为 USB Serial 配置，新增屏幕亮度设置，WiFi 配置变为可选。

**Architecture:** 固件端完全移除 NimBLE 协议栈，改用 Arduino USB CDC Serial 进行 JSON 行协议通信。Web 端改用 Web Serial API 连接 COM 端口。设备连接后自动发送当前配置，Web 端始终通过 USB 同步浏览器时间。

**Tech Stack:** ESP32-S3 + Arduino + USB CDC Serial + Web Serial API + ArduinoJson

---

## 文件结构

### 固件端（main 分支）

| 文件 | 责任 |
|------|------|
| `src/config_types.h` | 配置数据结构定义（增加 brightness） |
| `src/config_store.cpp` | Preferences 存储读写（增加 brightness） |
| `src/config_json.cpp` | JSON 字符串解析为 DeviceConfig（增加 brightness） |
| `src/config_validator.cpp` | 配置字段校验（增加 brightness 范围校验，WiFi 可选） |
| `src/usb_config_service.h` | USB 配置服务接口 |
| `src/usb_config_service.cpp` | USB 配置服务实现（JSON 行协议解析/发送） |
| `src/main.cpp` | 主程序：状态机、时钟显示、按钮处理、配置模式切换 |
| `platformio.ini` | 构建配置：移除 NimBLE，添加 ArduinoJson |

### Web 端（gh-pages 分支）

| 文件 | 责任 |
|------|------|
| `index.html` | 单页面：连接按钮 + 配置表单（含亮度滑块） |
| `app.js` | Web Serial API 连接、JSON 协议处理、表单交互 |
| `style.css` | 页面样式 |

---

## Task 1: 修改配置数据结构（增加 brightness 字段）

**Files:**
- Modify: `src/config_types.h`
- Modify: `src/config_store.cpp`
- Modify: `src/config_json.cpp`
- Modify: `src/config_validator.cpp`
- Test: `test/native/test_config_json/test_main.cpp`
- Test: `test/native/test_config_validator/test_main.cpp`

### Step 1: 修改 config_types.h 增加 brightness 字段

```cpp
#pragma once

#include <stdint.h>
#include <string>

enum class ConfigError : uint16_t {
    Ok = 0,
    JsonParseFailed = 2001,
    InvalidField = 2002,
    WifiConnectFailed = 3001,
    NtpSyncFailed = 3002,
    WifiScanFailed = 3003,
};

enum class ConfigState : uint8_t {
    Idle,
    ConfigReceived,
    Applying,
    Done,
    Error,
    ScanComplete = 9,
};

struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPassword;
    std::string timezone;
    std::string ntpServer;
    uint8_t brightness = 50;  // 1-100，默认 50
};
```

### Step 2: 修改 config_store.cpp 增加亮度读写

```cpp
#include "config_store.h"

bool ConfigStore::load(DeviceConfig& outConfig) {
    outConfig.wifiSsid = prefs_.getString("ssid", "").c_str();
    outConfig.wifiPassword = prefs_.getString("password", "").c_str();
    outConfig.timezone = prefs_.getString("timezone", "Asia/Shanghai").c_str();
    outConfig.ntpServer = prefs_.getString("ntp_server", "ntp.aliyun.com").c_str();
    outConfig.brightness = prefs_.getUChar("brightness", 50);
    return !outConfig.wifiSsid.empty();
}

bool ConfigStore::save(const DeviceConfig& config) {
    bool ok = true;
    ok = ok && prefs_.putString("ssid", config.wifiSsid.c_str()) > 0;
    ok = ok && prefs_.putString("password", config.wifiPassword.c_str()) >= 0;
    ok = ok && prefs_.putString("timezone", config.timezone.c_str()) > 0;
    ok = ok && prefs_.putString("ntp_server", config.ntpServer.c_str()) > 0;
    ok = ok && prefs_.putUChar("brightness", config.brightness) > 0;
    return ok;
}
```

### Step 3: 修改 config_json.cpp 增加 brightness 解析

保持现有简单字符串提取风格，增加整数字段提取：

```cpp
#include "config_json.h"

static bool extractStringField(const std::string& json, const char* key, std::string& out) {
    std::string pattern = std::string("\"") + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t firstQuote = json.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return false;
    }

    size_t secondQuote = json.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return false;
    }

    out = json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    return true;
}

static bool extractIntField(const std::string& json, const char* key, int& out) {
    std::string pattern = std::string("\"") + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) {
        start++;
    }

    size_t end = start;
    while (end < json.size() && (json[end] >= '0' && json[end] <= '9')) {
        end++;
    }

    if (start == end) {
        return false;
    }

    out = std::stoi(json.substr(start, end - start));
    return true;
}

bool parseConfigJson(const std::string& json, DeviceConfig& outConfig) {
    bool hasSsid = extractStringField(json, "wifiSsid", outConfig.wifiSsid);
    bool hasPassword = extractStringField(json, "wifiPassword", outConfig.wifiPassword);
    bool hasTimezone = extractStringField(json, "timezone", outConfig.timezone);
    bool hasNtp = extractStringField(json, "ntpServer", outConfig.ntpServer);

    int brightness = 50;
    bool hasBrightness = extractIntField(json, "brightness", brightness);
    if (hasBrightness) {
        outConfig.brightness = static_cast<uint8_t>(brightness);
    }

    // WiFi SSID 变为可选，但时区和 NTP 必填
    return hasTimezone && hasNtp;
}
```

### Step 4: 修改 config_validator.cpp 增加亮度校验，WiFi 可选

```cpp
#include "config_validator.h"

static bool isLenInRange(const std::string& s, size_t minLen, size_t maxLen) {
    return s.size() >= minLen && s.size() <= maxLen;
}

ConfigError validateConfig(const DeviceConfig& config) {
    // WiFi SSID 可选：如果填写了则校验长度
    if (!config.wifiSsid.empty() && !isLenInRange(config.wifiSsid, 1, 32)) {
        return ConfigError::InvalidField;
    }
    // WiFi 密码可选：如果填写了则校验长度
    if (!config.wifiPassword.empty() && config.wifiPassword.size() > 64) {
        return ConfigError::InvalidField;
    }
    if (!isLenInRange(config.timezone, 1, 64)) {
        return ConfigError::InvalidField;
    }
    if (!isLenInRange(config.ntpServer, 1, 128)) {
        return ConfigError::InvalidField;
    }
    if (config.brightness < 1 || config.brightness > 100) {
        return ConfigError::InvalidField;
    }
    return ConfigError::Ok;
}
```

### Step 5: 更新 config_json 测试

```cpp
#include "config_json.h"
#include "../../../src/config_json.cpp"

#include <assert.h>

int main() {
    DeviceConfig cfg;
    const bool ok = parseConfigJson(
        "{\"wifiSsid\":\"Home\",\"wifiPassword\":\"12345678\",\"timezone\":\"Asia/Shanghai\",\"ntpServer\":\"ntp.aliyun.com\",\"brightness\":75}",
        cfg);
    assert(ok);
    assert(cfg.wifiSsid == "Home");
    assert(cfg.wifiPassword == "12345678");
    assert(cfg.timezone == "Asia/Shanghai");
    assert(cfg.ntpServer == "ntp.aliyun.com");
    assert(cfg.brightness == 75);

    // WiFi 为空（可选）
    DeviceConfig noWifi;
    const bool ok2 = parseConfigJson(
        "{\"wifiSsid\":\"\",\"wifiPassword\":\"\",\"timezone\":\"Asia/Shanghai\",\"ntpServer\":\"ntp.aliyun.com\"}",
        noWifi);
    assert(ok2);
    assert(noWifi.wifiSsid == "");
    assert(noWifi.brightness == 50);  // 默认值

    // 缺少必填字段 timezone
    DeviceConfig bad;
    assert(!parseConfigJson("{\"wifiSsid\":\"Home\",\"ntpServer\":\"ntp.aliyun.com\"}", bad));
    return 0;
}
```

### Step 6: 更新 config_validator 测试

```cpp
#include "config_types.h"
#include "config_validator.h"
#include "../../../src/config_validator.cpp"

#include <assert.h>

int main() {
    // 完整配置
    DeviceConfig ok{"HomeWiFi", "12345678", "Asia/Shanghai", "ntp.aliyun.com", 50};
    assert(validateConfig(ok) == ConfigError::Ok);

    // WiFi 为空（可选）
    DeviceConfig noWifi{"", "", "Asia/Shanghai", "ntp.aliyun.com", 50};
    assert(validateConfig(noWifi) == ConfigError::Ok);

    // 亮度超限
    DeviceConfig badBright{"HomeWiFi", "12345678", "Asia/Shanghai", "ntp.aliyun.com", 101};
    assert(validateConfig(badBright) == ConfigError::InvalidField);

    // 时区为空
    DeviceConfig badTz{"HomeWiFi", "12345678", "", "ntp.aliyun.com", 50};
    assert(validateConfig(badTz) == ConfigError::InvalidField);

    // NTP 为空
    DeviceConfig badNtp{"HomeWiFi", "12345678", "Asia/Shanghai", "", 50};
    assert(validateConfig(badNtp) == ConfigError::InvalidField);

    return 0;
}
```

### Step 7: 运行 native 测试

```bash
cd /Users/kenn/PROJECTS/m5stack/matrix5
pio test -e native
```

Expected: 所有测试通过。

### Step 8: 提交

```bash
git add src/config_types.h src/config_store.cpp src/config_json.cpp src/config_validator.cpp \
       test/native/test_config_json/test_main.cpp test/native/test_config_validator/test_main.cpp
git commit -m "feat: add brightness field to DeviceConfig, make WiFi optional"
```

---

## Task 2: 创建 UsbConfigService

**Files:**
- Create: `src/usb_config_service.h`
- Create: `src/usb_config_service.cpp`

### Step 1: 创建 usb_config_service.h

```cpp
#pragma once

#include "config_types.h"

#include <cstring>
#include <functional>
#include <string>

struct UsbConfigCallbacks {
    std::function<void(const DeviceConfig&)> onApply;
    std::function<void()> onScanWifi;
    std::function<void()> onGetConfig;
};

class UsbConfigService {
   public:
    void begin(const UsbConfigCallbacks& callbacks);
    void loop();

    void sendConfig(const DeviceConfig* cfg);
    void sendScanResult(const std::string& json);
    void sendOk(const char* message);
    void sendError(ConfigError code, const char* message);

    bool hasPendingApply() const;
    DeviceConfig getPendingConfig() const;
    uint32_t getPendingTime() const;
    bool hasScanRequest() const;
    void clearPendingApply();
    void clearScanRequest();

   private:
    void processLine(const std::string& line);

    UsbConfigCallbacks callbacks_;
    bool hasPendingApply_ = false;
    bool hasScanRequest_ = false;
    DeviceConfig pendingConfig_;
    uint32_t pendingTime_ = 0;
    std::string rxBuffer_;
};
```

### Step 2: 创建 usb_config_service.cpp

```cpp
#include "usb_config_service.h"

#include <Arduino.h>
#include <ArduinoJson.h>

void UsbConfigService::begin(const UsbConfigCallbacks& callbacks) {
    callbacks_ = callbacks;
}

void UsbConfigService::loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            processLine(rxBuffer_);
            rxBuffer_.clear();
        } else {
            rxBuffer_ += c;
        }
    }
}

void UsbConfigService::processLine(const std::string& line) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, line.c_str());
    if (err) return;

    const char* cmd = doc["cmd"];
    if (!cmd) return;

    if (strcmp(cmd, "apply") == 0) {
        JsonObject config = doc["config"];
        pendingConfig_.wifiSsid = config["wifiSsid"] | "";
        pendingConfig_.wifiPassword = config["wifiPassword"] | "";
        pendingConfig_.timezone = config["timezone"] | "Asia/Shanghai";
        pendingConfig_.ntpServer = config["ntpServer"] | "ntp.aliyun.com";
        pendingConfig_.brightness = config["brightness"] | 50;
        pendingTime_ = doc["time"] | 0;
        hasPendingApply_ = true;
        if (callbacks_.onApply) callbacks_.onApply(pendingConfig_);
    } else if (strcmp(cmd, "scan_wifi") == 0) {
        hasScanRequest_ = true;
        if (callbacks_.onScanWifi) callbacks_.onScanWifi();
    } else if (strcmp(cmd, "get_config") == 0) {
        if (callbacks_.onGetConfig) callbacks_.onGetConfig();
    }
}

void UsbConfigService::sendConfig(const DeviceConfig* cfg) {
    StaticJsonDocument<512> doc;
    doc["type"] = "config";
    if (cfg) {
        JsonObject data = doc.createNestedObject("data");
        data["wifiSsid"] = cfg->wifiSsid.c_str();
        data["wifiPassword"] = cfg->wifiPassword.c_str();
        data["timezone"] = cfg->timezone.c_str();
        data["ntpServer"] = cfg->ntpServer.c_str();
        data["brightness"] = cfg->brightness;
    } else {
        doc["data"] = nullptr;
    }
    serializeJson(doc, Serial);
    Serial.println();
}

void UsbConfigService::sendScanResult(const std::string& json) {
    Serial.println(json.c_str());
}

void UsbConfigService::sendOk(const char* message) {
    StaticJsonDocument<128> doc;
    doc["type"] = "ok";
    doc["message"] = message;
    serializeJson(doc, Serial);
    Serial.println();
}

void UsbConfigService::sendError(ConfigError code, const char* message) {
    StaticJsonDocument<128> doc;
    doc["type"] = "error";
    doc["code"] = static_cast<int>(code);
    doc["message"] = message;
    serializeJson(doc, Serial);
    Serial.println();
}

bool UsbConfigService::hasPendingApply() const { return hasPendingApply_; }
DeviceConfig UsbConfigService::getPendingConfig() const { return pendingConfig_; }
uint32_t UsbConfigService::getPendingTime() const { return pendingTime_; }
bool UsbConfigService::hasScanRequest() const { return hasScanRequest_; }
void UsbConfigService::clearPendingApply() { hasPendingApply_ = false; }
void UsbConfigService::clearScanRequest() { hasScanRequest_ = false; }
```

### Step 3: 提交

```bash
git add src/usb_config_service.h src/usb_config_service.cpp
git commit -m "feat: add UsbConfigService with JSON line protocol"
```

---

## Task 3: 重写 main.cpp（移除 BLE，集成 USB Serial）

**Files:**
- Modify: `src/main.cpp`

### Step 1: 替换 main.cpp 顶部 include 和全局变量

```cpp
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

DeviceConfig pendingConfig{};
bool hasPendingConfig = false;
bool configModeActive = false;
bool wifiScanRequested = false;
bool wifiScanInProgress = false;

M5Canvas sprite(&M5.Display);
```

### Step 2: 替换 main.cpp 辅助函数和显示函数

保留原有的 `charIndex`, `drawDot`, `drawBackgroundDots`, `drawChar`, `drawColon`, `drawClock` 函数不变。

删除 `drawBleScreen`, `drawPairedConfirmScreen`，替换为新的配置模式显示函数：

```cpp
static void drawConfigScreen() {
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString("USB CONFIG", M5.Display.width() / 2, M5.Display.height() / 2 - 10);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Connect via USB", M5.Display.width() / 2, M5.Display.height() / 2 + 15);
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
```

### Step 3: 保留 WiFi 扫描相关函数

保留 `escapeJsonString` 和 `buildScanResultJson` 函数不变（这些在之前的 WiFi 扫描功能中已经添加）。

### Step 4: 添加 enterConfigMode 函数

```cpp
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
```

### Step 5: 重写 setup() 函数

```cpp
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
        // 应用保存的亮度
        M5.Display.setBrightness(cfgData.brightness * 255 / 100);
        // WiFi 可能为空，只在有 SSID 时连接
        if (!cfgData.wifiSsid.empty()) {
            applyNetworkConfig(cfgData);
        }
        configModeActive = false;
    } else {
        // 无有效配置，使用默认亮度并进入配置模式
        M5.Display.setBrightness(50 * 255 / 100);
        enterConfigMode();
    }
}
```

### Step 6: 重写 loop() 函数

```cpp
void loop() {
    if (configModeActive) {
        usbConfigService.loop();

        // 处理 apply 请求
        if (hasPendingConfig) {
            hasPendingConfig = false;
            ConfigError valid = validateConfig(pendingConfig);
            if (valid != ConfigError::Ok) {
                usbConfigService.sendError(valid, "invalid_config");
            } else if (!configStore.save(pendingConfig)) {
                usbConfigService.sendError(ConfigError::InvalidField, "save_failed");
            } else {
                // 应用亮度
                M5.Display.setBrightness(pendingConfig.brightness * 255 / 100);

                // 应用时间（如果有时间戳）
                uint32_t ts = usbConfigService.getPendingTime();
                if (ts > 0) {
                    timeval tv = { static_cast<time_t>(ts), 0 };
                    settimeofday(&tv, nullptr);
                }

                // 应用网络（WiFi 可能为空）
                ConfigError netErr = ConfigError::Ok;
                if (!pendingConfig.wifiSsid.empty()) {
                    netErr = applyNetworkConfig(pendingConfig);
                }

                if (netErr == ConfigError::Ok) {
                    usbConfigService.sendOk("applied");
                    delay(500);
                    configModeActive = false;
                    drawStatusScreen("SYNC OK", "Config applied");
                    delay(1000);
                } else {
                    usbConfigService.sendError(netErr, "apply_failed");
                }
            }
        }

        // WiFi 异步扫描（复用现有逻辑）
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
        // 正常模式：显示时钟
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
    }

    M5.update();

    // 长按 btnA + btnB 5秒进入配置模式
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
```

### Step 7: 编译验证

```bash
cd /Users/kenn/PROJECTS/m5stack/matrix5
pio run -e m5stick-s3
```

注意：此时编译会失败，因为 NimBLE 头文件缺失（还未删除），但先验证 main.cpp 语法。

### Step 8: 提交

```bash
git add src/main.cpp
git commit -m "feat: replace BLE with USB Serial in main.cpp"
```

---

## Task 4: 删除 BLE 文件，更新 platformio.ini

**Files:**
- Delete: `src/ble_config_service.h`
- Delete: `src/ble_config_service.cpp`
- Delete: `src/paired_device_store.h`
- Delete: `src/paired_device_store.cpp`
- Delete: `src/pairing_code.h`
- Delete: `src/pairing_code.cpp`
- Delete: `src/ble_display_text.h`
- Delete: `src/ble_display_text.cpp`
- Modify: `platformio.ini`
- Delete: `test/native/test_ble_display_text/`
- Delete: `test/native/test_paired_device_store/`
- Delete: `test/native/test_pairing_code/`

### Step 1: 删除 BLE 相关源文件

```bash
cd /Users/kenn/PROJECTS/m5stack/matrix5
rm -f src/ble_config_service.h src/ble_config_service.cpp
rm -f src/paired_device_store.h src/paired_device_store.cpp
rm -f src/pairing_code.h src/pairing_code.cpp
rm -f src/ble_display_text.h src/ble_display_text.cpp
```

### Step 2: 修改 platformio.ini 移除 NimBLE，添加 ArduinoJson

```ini
[env:m5stick-s3]
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.partitions = default_8MB.csv
board_build.arduino.memory_type = qio_opi
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    https://github.com/m5stack/M5Unified
    https://github.com/m5stack/M5PM1
    bblanchon/ArduinoJson @ ^7.0.0

[env:native]
platform = native
test_build_src = no
build_flags =
    -Isrc
```

### Step 3: 删除 BLE 相关测试目录

```bash
rm -rf test/native/test_ble_display_text
rm -rf test/native/test_paired_device_store
rm -rf test/native/test_pairing_code
```

### Step 4: 编译验证

```bash
pio run -e m5stick-s3
```

Expected: 编译成功。

### Step 5: 运行 native 测试

```bash
pio test -e native
```

Expected: 剩余测试全部通过。

### Step 6: 提交

```bash
git add -A
git commit -m "refactor: remove BLE stack and pairing system, add ArduinoJson dependency"
```

---

## Task 5: 重写 Web 端（gh-pages 分支）

**Files:**
- Modify: `index.html`
- Modify: `app.js`
- Modify: `style.css`

**注意：** 此任务需要在 `gh-pages` 分支上执行。开始前先切换分支：

```bash
git checkout gh-pages
```

### Step 1: 重写 index.html

```html
<!doctype html>
<html lang="zh-CN">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>matrix5 配置</title>
    <link rel="stylesheet" href="./style.css" />
  </head>
  <body>
    <main class="container">
      <h1>matrix5 配置</h1>

      <!-- 连接区域 -->
      <div id="connectSection" class="card">
        <button id="connectBtn">连接设备</button>
        <p id="connectStatus" class="hint">未连接</p>
      </div>

      <!-- 配置表单（连接后显示） -->
      <div id="configSection" class="card" style="display:none;">
        <label>屏幕亮度 <span id="brightnessValue">50%</span></label>
        <input type="range" id="brightness" min="1" max="100" value="50" />

        <label>WiFi 名称（可选）</label>
        <input id="wifiSsid" placeholder="留空则不连接 WiFi" />
        <div id="wifiScanStatus" class="scan-status" style="display:none;"></div>
        <div id="wifiScanList" class="scan-list"></div>
        <button id="scanWifiBtn" class="btn-secondary">扫描附近 WiFi</button>

        <label>WiFi 密码</label>
        <input id="wifiPassword" type="password" placeholder="WiFi 密码" />

        <label>时区</label>
        <select id="timezone">
          <option value="Asia/Shanghai">Asia/Shanghai (中国/上海)</option>
          <option value="Asia/Hong_Kong">Asia/Hong_Kong (香港)</option>
          <option value="Asia/Taipei">Asia/Taipei (台北)</option>
          <option value="Asia/Tokyo">Asia/Tokyo (日本/东京)</option>
          <option value="Asia/Seoul">Asia/Seoul (韩国/首尔)</option>
          <option value="Asia/Singapore">Asia/Singapore (新加坡)</option>
          <option value="Asia/Bangkok">Asia/Bangkok (泰国/曼谷)</option>
          <option value="Asia/Dubai">Asia/Dubai (阿联酋/迪拜)</option>
          <option value="Europe/London">Europe/London (英国/伦敦)</option>
          <option value="Europe/Paris">Europe/Paris (法国/巴黎)</option>
          <option value="Europe/Berlin">Europe/Berlin (德国/柏林)</option>
          <option value="Europe/Moscow">Europe/Moscow (俄罗斯/莫斯科)</option>
          <option value="America/New_York">America/New_York (美国/纽约)</option>
          <option value="America/Los_Angeles">America/Los_Angeles (美国/洛杉矶)</option>
          <option value="America/Chicago">America/Chicago (美国/芝加哥)</option>
          <option value="America/Toronto">America/Toronto (加拿大/多伦多)</option>
          <option value="Australia/Sydney">Australia/Sydney (澳大利亚/悉尼)</option>
          <option value="Pacific/Auckland">Pacific/Auckland (新西兰/奥克兰)</option>
          <option value="UTC">UTC</option>
        </select>

        <label>NTP 服务器</label>
        <input id="ntpServer" value="ntp.aliyun.com" />

        <button id="applyBtn">应用配置</button>
        <div class="step-status" id="applyStatus"></div>
      </div>
    </main>
    <script type="module" src="./app.js"></script>
  </body>
</html>
```

### Step 2: 重写 app.js

```javascript
const $ = (id) => document.getElementById(id);
const te = new TextEncoder();
const td = new TextDecoder();

let port = null;
let reader = null;
let writer = null;
let readLoopRunning = false;

/* ================================================================
   USB Serial Connection
   ================================================================ */

async function connectDevice() {
  try {
    port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: 0x303a }],  // Espressif (ESP32-S3)
    });
    await port.open({ baudRate: 115200 });

    writer = port.writable.getWriter();

    $("connectBtn").style.display = "none";
    $("connectStatus").textContent = "已连接，等待设备响应...";
    $("connectStatus").className = "hint ok";

    startReadLoop();

    // 请求设备发送当前配置
    await sendJson({ cmd: "get_config" });
  } catch (e) {
    $("connectStatus").textContent = "连接失败: " + e.message;
    $("connectStatus").className = "hint err";
  }
}

async function startReadLoop() {
  if (readLoopRunning) return;
  readLoopRunning = true;

  let buffer = "";

  while (port && port.readable) {
    try {
      reader = port.readable.getReader();
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += td.decode(value, { stream: true });

        let lines = buffer.split("\n");
        buffer = lines.pop();
        for (const line of lines) {
          if (line.trim()) handleMessage(line.trim());
        }
      }
    } catch (e) {
      console.error("Read error:", e);
    } finally {
      if (reader) {
        reader.releaseLock();
        reader = null;
      }
    }
  }

  readLoopRunning = false;
  onDisconnected();
}

function handleMessage(line) {
  try {
    const msg = JSON.parse(line);
    if (msg.type === "config") {
      if (msg.data) {
        $("brightness").value = msg.data.brightness || 50;
        $("brightnessValue").textContent = ($("brightness").value) + "%";
        $("wifiSsid").value = msg.data.wifiSsid || "";
        $("wifiPassword").value = msg.data.wifiPassword || "";
        $("timezone").value = msg.data.timezone || "Asia/Shanghai";
        $("ntpServer").value = msg.data.ntpServer || "ntp.aliyun.com";
      }
      $("connectStatus").textContent = "配置已加载";
      $("configSection").style.display = "block";
    } else if (msg.type === "scan_result") {
      renderWifiScanList(msg.networks || []);
    } else if (msg.type === "ok") {
      $("applyStatus").className = "step-status ok";
      $("applyStatus").textContent = "配置已应用成功";
    } else if (msg.type === "error") {
      $("applyStatus").className = "step-status err";
      $("applyStatus").textContent = `错误码 ${msg.code}: ${msg.message}`;
    }
  } catch (e) {
    console.error("Parse error:", line, e);
  }
}

async function sendJson(obj) {
  if (!writer) return;
  const data = JSON.stringify(obj) + "\n";
  await writer.write(te.encode(data));
}

function onDisconnected() {
  $("connectBtn").style.display = "block";
  $("connectStatus").textContent = "设备已断开";
  $("connectStatus").className = "hint err";
  $("configSection").style.display = "none";
  port = null;
  writer = null;
}

/* ================================================================
   WiFi Scan List
   ================================================================ */

async function onScanWifi() {
  $("wifiScanStatus").style.display = "block";
  $("wifiScanStatus").textContent = "正在扫描...";
  $("wifiScanList").innerHTML = "";
  await sendJson({ cmd: "scan_wifi" });
}

function renderWifiScanList(networks) {
  const listEl = $("wifiScanList");
  const statusEl = $("wifiScanStatus");

  if (networks.length === 0) {
    statusEl.textContent = "未找到可用 Wi-Fi 网络";
    listEl.innerHTML = "";
    return;
  }

  statusEl.textContent = "点击选择要连接的网络：";
  listEl.innerHTML = networks
    .map(
      (n, idx) => `
    <div class="scan-item" data-ssid="${escapeHtml(n.ssid)}" data-idx="${idx}">
      <span class="ssid">${escapeHtml(n.ssid)}</span>
      <span class="rssi">${n.rssi} dBm</span>
    </div>
  `
    )
    .join("");

  listEl.querySelectorAll(".scan-item").forEach((item) => {
    item.addEventListener("click", (e) => {
      const ssid = e.currentTarget.dataset.ssid;
      $("wifiSsid").value = ssid;
      listEl.querySelectorAll(".scan-item").forEach((el) => el.classList.remove("selected"));
      e.currentTarget.classList.add("selected");
    });
  });
}

function escapeHtml(str) {
  return str
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

/* ================================================================
   Apply Config
   ================================================================ */

async function onApplyConfig() {
  const payload = {
    wifiSsid: $("wifiSsid").value.trim(),
    wifiPassword: $("wifiPassword").value,
    timezone: $("timezone").value,
    ntpServer: $("ntpServer").value.trim(),
    brightness: parseInt($("brightness").value, 10),
  };

  if (!payload.timezone || !payload.ntpServer) {
    $("applyStatus").className = "step-status err";
    $("applyStatus").textContent = "时区、NTP 服务器不能为空";
    return;
  }

  $("applyStatus").className = "step-status warn";
  $("applyStatus").textContent = "正在应用配置...";

  try {
    await sendJson({
      cmd: "apply",
      config: payload,
      time: Math.floor(Date.now() / 1000),
    });
  } catch (e) {
    $("applyStatus").className = "step-status err";
    $("applyStatus").textContent = `下发失败: ${e.message}`;
  }
}

/* ================================================================
   Brightness Slider
   ================================================================ */

$("brightness").addEventListener("input", (e) => {
  $("brightnessValue").textContent = e.target.value + "%";
});

/* ================================================================
   Compatibility Check & Init
   ================================================================ */

function checkCompat() {
  if (!navigator.serial) {
    $("connectStatus").textContent =
      "当前浏览器不支持 Web Serial，请使用 Chrome / Edge 桌面版";
    $("connectBtn").disabled = true;
  }
}

checkCompat();
$("connectBtn").addEventListener("click", connectDevice);
$("scanWifiBtn").addEventListener("click", onScanWifi);
$("applyBtn").addEventListener("click", onApplyConfig);
```

### Step 3: 调整 style.css

在现有样式基础上，移除设备列表/配对相关的样式，添加亮度滑块样式：

```css
/* 在现有 style.css 基础上修改 */

/* 移除以下不再使用的样式（如果存在）：
   - .device-card, .device-header, .device-meta, .device-actions
   - 与设备列表、配对码输入相关的样式
*/

/* 新增/调整 */
#connectSection {
  text-align: center;
  padding: 32px 24px;
}

#connectSection button {
  font-size: 16px;
  padding: 14px 32px;
}

#brightness {
  width: 100%;
  margin: 8px 0 16px;
}

#brightnessValue {
  color: var(--accent);
  font-weight: 500;
}

.btn-secondary {
  background: transparent;
  border: 1px solid var(--border-light);
  color: var(--text);
  margin-bottom: 16px;
}

.btn-secondary:hover {
  border-color: var(--accent);
  color: var(--accent);
}

/* 保留现有的 .scan-status, .scan-list, .scan-item 样式 */
```

### Step 4: 提交

```bash
git add index.html app.js style.css
git commit -m "feat(web): rewrite for USB Serial config, add brightness control"
```

---

## Self-Review

### 1. Spec Coverage

| 设计文档要求 | 实现任务 |
|-------------|---------|
| USB Serial JSON 行协议 | Task 2 (UsbConfigService) |
| 设备连接后自动发送配置 | Task 3 (main.cpp onGetConfig callback) |
| WiFi 可选 | Task 1 (config_validator WiFi 可选) + Task 3 (main.cpp 条件连接) |
| 亮度设置 1-100 | Task 1 (config_types) + Task 3 (main.cpp setBrightness) + Task 5 (Web 滑块) |
| 时间同步 | Task 3 (main.cpp settimeofday) + Task 5 (Web Date.now()/1000) |
| 移除 BLE | Task 4 (删除文件 + platformio.ini) |
| Web 简化 | Task 5 (重写 index.html/app.js) |
| WiFi 扫描保留 | Task 3 (main.cpp 保留扫描逻辑) + Task 5 (Web 保留渲染逻辑) |

**无遗漏。**

### 2. Placeholder Scan

无 TBD/TODO/"implement later"/"fill in details" 等占位符。

### 3. Type Consistency

- `DeviceConfig.brightness` 为 `uint8_t`（1-100），前后一致
- `UsbConfigService` 接口与设计文档一致
- JSON 字段名前后一致（brightness, wifiSsid 等）

---

## 执行方式

Plan complete and saved to `docs/superpowers/plans/2026-05-11-usb-serial-config-plan.md`.

**Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
