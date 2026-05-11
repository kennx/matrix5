# USB Serial 配置改造 + 亮度设置 设计文档

## 目标

推翻现有 BLE（蓝牙低功耗）配置方案，改为通过 USB Serial（Web Serial API）进行设备配置。简化流程：USB 连接后直接开始配置，无需配对码。重新配置时设备自动发送当前配置到 Web 端填充表单。新增屏幕亮度设置，默认 50%。

## 架构

固件端完全移除 NimBLE-Arduino BLE 协议栈，改用 Arduino `Serial`（USB CDC）进行通信。Web 端完全移除 Web Bluetooth API，改用 Web Serial API。采用简单的 JSON 行协议（每行一个 JSON 消息，以 `\n` 结尾）。WiFi 配置变为可选，Web 端始终通过 USB 发送浏览器当前时间戳给设备。

## 技术栈

- 固件：ESP32-S3 + Arduino + USB CDC Serial（无需 NimBLE）
- Web：Web Serial API + 原生 JavaScript

---

## USB Serial 协议（JSON 行协议）

每行一个 JSON 消息，以 `\n` 结尾。波特率 115200。

### 设备 → Web（上行）

**1. 当前配置（连接后自动发送）**

```json
{"type":"config","data":{"wifiSsid":"MyWiFi","wifiPassword":"secret","timezone":"Asia/Shanghai","ntpServer":"ntp.aliyun.com","brightness":50}}
```

无配置时：
```json
{"type":"config","data":null}
```

**2. WiFi 扫描结果**

```json
{"type":"scan_result","networks":[{"ssid":"WiFi1","rssi":-45},{"ssid":"WiFi2","rssi":-62}]}
```

无网络：
```json
{"type":"scan_result","networks":[]}
```

**3. 操作成功**

```json
{"type":"ok","message":"applied"}
```

**4. 错误**

```json
{"type":"error","code":3001,"message":"wifi_connect_failed"}
```

### Web → 设备（下行）

**1. 应用配置（同时下发配置和浏览器当前时间戳）**

```json
{"cmd":"apply","config":{"wifiSsid":"MyWiFi","wifiPassword":"secret","timezone":"Asia/Shanghai","ntpServer":"ntp.aliyun.com","brightness":75},"time":1746878923}
```

WiFi 留空（不连接网络）：
```json
{"cmd":"apply","config":{"wifiSsid":"","wifiPassword":"","timezone":"Asia/Shanghai","ntpServer":"ntp.aliyun.com","brightness":75},"time":1746878923}
```

**2. 请求扫描 WiFi**

```json
{"cmd":"scan_wifi"}
```

**3. 请求获取当前配置**

```json
{"cmd":"get_config"}
```

---

## 固件端设计（main 分支）

### 配置数据结构变更

`src/config_types.h`：

```cpp
struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPassword;
    std::string timezone;
    std::string ntpServer;
    uint8_t brightness = 50;  // 1-100，默认 50
};
```

### ConfigStore 亮度存储

`src/config_store.cpp` 增加：

```cpp
bool ConfigStore::save(const DeviceConfig& cfg) {
    // ... 原有字段保存 ...
    prefs_.putUInt("brightness", cfg.brightness);
    return true;
}

bool ConfigStore::load(DeviceConfig& cfg) {
    // ... 原有字段读取 ...
    cfg.brightness = prefs_.getUInt("brightness", 50);
    return hasSsid;
}
```

### 亮度校验

`src/config_validator.cpp` 增加：

```cpp
ConfigError validateConfig(const DeviceConfig& cfg) {
    // ... 原有校验 ...
    if (cfg.brightness < 1 || cfg.brightness > 100) {
        return ConfigError::InvalidField;
    }
    return ConfigError::Ok;
}
```

### UsbConfigService 接口

`src/usb_config_service.h`：

```cpp
#pragma once

#include "config_types.h"
#include <string>
#include <functional>

struct UsbConfigCallbacks {
    std::function<void(const DeviceConfig&)> onApply;      // 收到 apply 命令
    std::function<void()> onScanWifi;                       // 收到 scan_wifi 命令
    std::function<void()> onGetConfig;                     // 收到 get_config 命令
};

class UsbConfigService {
public:
    void begin(const UsbConfigCallbacks& callbacks);
    void loop();  // 每帧调用，处理接收和发送
    void sendConfig(const DeviceConfig* cfg);              // 发送当前配置（cfg=null 表示无配置）
    void sendScanResult(const std::string& json);          // 发送 WiFi 扫描结果
    void sendOk(const char* message);                      // 发送成功响应
    void sendError(ConfigError code, const char* message); // 发送错误
    bool hasPendingApply() const;
    DeviceConfig getPendingConfig() const;
    uint32_t getPendingTime() const;  // 浏览器发送的时间戳
    bool hasScanRequest() const;
    void clearPendingApply();
    void clearScanRequest();

private:
    UsbConfigCallbacks callbacks_;
    bool hasPendingApply_ = false;
    bool hasScanRequest_ = false;
    DeviceConfig pendingConfig_;
    uint32_t pendingTime_ = 0;
    std::string rxBuffer_;
};
```

### UsbConfigService 实现

`src/usb_config_service.cpp`：

```cpp
#include "usb_config_service.h"
#include <ArduinoJson.h>

void UsbConfigService::begin(const UsbConfigCallbacks& callbacks) {
    callbacks_ = callbacks;
    Serial.begin(115200);
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
    DeserializationError err = deserializeJson(doc, line);
    if (err) return;  // 忽略解析失败的行

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
        data["wifiSsid"] = cfg->wifiSsid;
        data["wifiPassword"] = cfg->wifiPassword;
        data["timezone"] = cfg->timezone;
        data["ntpServer"] = cfg->ntpServer;
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

### main.cpp 状态机

```cpp
UsbConfigService usbConfigService;
bool configModeActive = false;

// 进入配置模式
void enterConfigMode() {
    configModeActive = true;
    M5.Display.fillScreen(BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString("USB CONFIG", M5.Display.width() / 2, M5.Display.height() / 2);
    
    UsbConfigCallbacks callbacks;
    callbacks.onApply = [&](const DeviceConfig& cfg) {
        // apply 在 loop 中处理
    };
    callbacks.onScanWifi = [&]() {
        // scan 在 loop 中处理
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
    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);
    
    sprite.createSprite(M5.Display.width(), M5.Display.height());
    
    prefs.begin("m5stick", false);
    
    DeviceConfig cfgData;
    if (configStore.load(cfgData) && validateConfig(cfgData) == ConfigError::Ok) {
        // 有有效配置，应用亮度并启动时钟
        M5.Display.setBrightness(cfgData.brightness * 255 / 100);
        applyNetworkConfig(cfgData);  // WiFi 可能为空，此时不连接
        configModeActive = false;
    } else {
        // 无配置，进入配置模式
        enterConfigMode();
    }
}

void loop() {
    if (configModeActive) {
        usbConfigService.loop();
        
        // 处理 apply
        if (usbConfigService.hasPendingApply()) {
            usbConfigService.clearPendingApply();
            DeviceConfig cfg = usbConfigService.getPendingConfig();
            ConfigError valid = validateConfig(cfg);
            if (valid != ConfigError::Ok) {
                usbConfigService.sendError(valid, "invalid_config");
            } else if (!configStore.save(cfg)) {
                usbConfigService.sendError(ConfigError::InvalidField, "save_failed");
            } else {
                // 应用亮度
                M5.Display.setBrightness(cfg.brightness * 255 / 100);
                
                // 应用时间（如果有时间戳）
                uint32_t ts = usbConfigService.getPendingTime();
                if (ts > 0) {
                    timeval tv = { static_cast<time_t>(ts), 0 };
                    settimeofday(&tv, nullptr);
                }
                
                // 应用网络（WiFi 可能为空）
                ConfigError netErr = ConfigError::Ok;
                if (!cfg.wifiSsid.empty()) {
                    netErr = applyNetworkConfig(cfg);
                }
                
                if (netErr == ConfigError::Ok) {
                    usbConfigService.sendOk("applied");
                    delay(500);
                    configModeActive = false;
                } else {
                    usbConfigService.sendError(netErr, "apply_failed");
                }
            }
        }
        
        // 处理 WiFi 扫描（复用现有异步扫描逻辑）
        if (usbConfigService.hasScanRequest()) {
            usbConfigService.clearScanRequest();
            WiFi.scanNetworks(true);
        }
        // ... 现有扫描完成处理，改为通过 usbConfigService.sendScanResult() 发送 ...
        
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
                strncpy(buf, "00:00:00", sizeof(buf));
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

### 删除的文件

- `src/ble_config_service.h`
- `src/ble_config_service.cpp`
- `src/paired_device_store.h`
- `src/paired_device_store.cpp`
- `src/pairing_code.h`
- `src/pairing_code.cpp`
- `src/ble_display_text.h`
- `src/ble_display_text.cpp`

### platformio.ini 变更

移除 NimBLE-Arduino 依赖：

```ini
lib_deps =
    ; 移除：h2zero/NimBLE-Arduino@^1.4.0
    m5stack/M5Unified@^0.1.16
    bblanchon/ArduinoJson@^7.0.0
```

---

## Web 端设计（gh-pages 分支）

### 页面结构

简化为单页面，移除设备列表、配对码、删除功能：

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

### JavaScript 核心逻辑

```javascript
const $ = (id) => document.getElementById(id);

let port = null;
let reader = null;
let writer = null;
let readLoopRunning = false;

// 连接设备
async function connectDevice() {
  try {
    port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: 0x303a }]  // Espressif (ESP32-S3)
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

// 读取循环
async function startReadLoop() {
  if (readLoopRunning) return;
  readLoopRunning = true;
  
  const decoder = new TextDecoder();
  let buffer = "";
  
  while (port && port.readable) {
    try {
      reader = port.readable.getReader();
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += decoder.decode(value, { stream: true });
        
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

// 处理设备消息
function handleMessage(line) {
  try {
    const msg = JSON.parse(line);
    if (msg.type === "config") {
      // 收到设备当前配置，填充表单
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

// 发送 JSON 到设备
async function sendJson(obj) {
  if (!writer) return;
  const data = JSON.stringify(obj) + "\n";
  await writer.write(new TextEncoder().encode(data));
}

// 断开处理
function onDisconnected() {
  $("connectBtn").style.display = "block";
  $("connectStatus").textContent = "设备已断开";
  $("connectStatus").className = "hint err";
  $("configSection").style.display = "none";
  port = null;
  writer = null;
}

// 应用配置
async function onApply() {
  const config = {
    wifiSsid: $("wifiSsid").value.trim(),
    wifiPassword: $("wifiPassword").value,
    timezone: $("timezone").value,
    ntpServer: $("ntpServer").value.trim(),
    brightness: parseInt($("brightness").value, 10)
  };
  
  $("applyStatus").className = "step-status warn";
  $("applyStatus").textContent = "正在应用配置...";
  
  await sendJson({
    cmd: "apply",
    config: config,
    time: Math.floor(Date.now() / 1000)
  });
}

// 扫描 WiFi
async function onScanWifi() {
  $("wifiScanStatus").style.display = "block";
  $("wifiScanStatus").textContent = "正在扫描...";
  $("wifiScanList").innerHTML = "";
  await sendJson({ cmd: "scan_wifi" });
}

// 渲染 WiFi 扫描列表（复用现有逻辑）
function renderWifiScanList(networks) {
  // ... 复用现有 renderWifiScanList 逻辑 ...
}

// 亮度滑块实时更新显示
$("brightness").addEventListener("input", (e) => {
  $("brightnessValue").textContent = e.target.value + "%";
});

// 浏览器兼容性检查
function checkCompat() {
  if (!navigator.serial) {
    $("connectStatus").textContent = "当前浏览器不支持 Web Serial，请使用 Chrome / Edge 桌面版";
    $("connectBtn").disabled = true;
  }
}

// 初始化
checkCompat();
$("connectBtn").addEventListener("click", connectDevice);
$("applyBtn").addEventListener("click", onApply);
$("scanWifiBtn").addEventListener("click", onScanWifi);
```

---

## 亮度设置

- **范围**：1-100（百分比）
- **默认值**：50
- **固件映射**：`M5.Display.setBrightness(brightness * 255 / 100)`
- **Web 控件**：`<input type="range" min="1" max="100">` + 百分比显示
- **生效时机**：配置保存后立即生效，无需重启
- **首次启动**：无配置时使用默认亮度 50%

---

## 文件变更清单

### 固件端（main 分支）

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/config_types.h` | 修改 | `DeviceConfig` 增加 `brightness` 字段 |
| `src/config_store.cpp` | 修改 | 增加亮度读写，默认 50 |
| `src/config_json.cpp` | 修改 | 增加 brightness JSON 序列化/反序列化 |
| `src/config_validator.cpp` | 修改 | 增加亮度范围校验 (1-100) |
| `src/main.cpp` | 大改 | 移除 BLE 代码，集成 UsbConfigService，处理亮度应用 |
| `src/usb_config_service.h` | 新增 | USB 配置服务接口 |
| `src/usb_config_service.cpp` | 新增 | USB 配置服务实现（JSON 行协议解析） |
| `src/ble_config_service.h` | 删除 | BLE 服务头文件 |
| `src/ble_config_service.cpp` | 删除 | BLE 服务实现 |
| `src/paired_device_store.h` | 删除 | 配对设备存储 |
| `src/paired_device_store.cpp` | 删除 | 配对设备存储实现 |
| `src/pairing_code.h` | 删除 | 配对码管理 |
| `src/pairing_code.cpp` | 删除 | 配对码管理实现 |
| `src/ble_display_text.h` | 删除 | BLE 显示文本 |
| `src/ble_display_text.cpp` | 删除 | BLE 显示文本实现 |
| `platformio.ini` | 修改 | 移除 NimBLE-Arduino 依赖 |
| `test/` | 修改 | 删除 BLE/配对相关测试 |

### Web 端（gh-pages 分支）

| 文件 | 操作 | 说明 |
|------|------|------|
| `index.html` | 重写 | 简化为单页面，移除设备列表/配对码/删除功能，增加亮度滑块 |
| `app.js` | 重写 | 移除 Web Bluetooth，改用 Web Serial API |
| `style.css` | 修改 | 调整样式适配新页面结构 |
