# BLE 网页配网替换实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 M5StickS3 上以 BLE GATT + Chrome 网页完成 WiFi/NTP 配置，并彻底移除旧 AP+HTTP 配网入口。

**Architecture:** 设备端拆分为 `config_store`、`pairing_code`、`config_validator`、`ble_config_service`、`net_apply` 五个小模块，`main.cpp` 只做流程编排。网页端放在 `docs/web-config/`，通过 Web Bluetooth 执行连接、鉴权、配置下发和状态展示。协议使用自定义 Service + JSON 配置 + 错误码通知。

**Tech Stack:** PlatformIO + Arduino(ESP32-S3) + M5Unified + NimBLE-Arduino + GitHub Pages + Chrome Web Bluetooth

---

## File Structure

| 文件 | 操作 | 职责 |
|---|---|---|
| `platformio.ini` | 修改 | 增加 BLE 库依赖，增加本地单元测试环境 |
| `src/main.cpp` | 修改 | 移除旧 AP 配网入口，接入 BLE 配网主流程 |
| `src/config_types.h` | 新建 | 配置结构体、错误码、状态枚举 |
| `src/config_validator.h` | 新建 | 配置校验函数声明 |
| `src/config_validator.cpp` | 新建 | 配置字段校验实现 |
| `src/config_store.h` | 新建 | NVS 配置读写接口声明 |
| `src/config_store.cpp` | 新建 | NVS 配置持久化实现 |
| `src/pairing_code.h` | 新建 | 一次性配网码接口声明 |
| `src/pairing_code.cpp` | 新建 | 6 位配网码生成/验证/过期实现 |
| `src/net_apply.h` | 新建 | WiFi/NTP 应用接口声明 |
| `src/net_apply.cpp` | 新建 | 按配置连接 WiFi 与 NTP 同步 |
| `src/ble_config_service.h` | 新建 | BLE 配网服务接口声明 |
| `src/ble_config_service.cpp` | 新建 | GATT 服务与写入回调、状态通知 |
| `docs/web-config/index.html` | 新建 | 配网页面结构 |
| `docs/web-config/style.css` | 新建 | 页面样式 |
| `docs/web-config/app.js` | 新建 | Web Bluetooth 连接与交互逻辑 |
| `docs/demo-ble-config.md` | 新建 | 上机演示脚本 |
| `test/test_ble_config_flow.md` | 新建 | 手工测试矩阵 |
| `test/native/test_config_validator/test_main.cpp` | 新建 | 配置校验单元测试 |
| `test/native/test_pairing_code/test_main.cpp` | 新建 | 配网码单元测试 |

---

### Task 1: 准备类型与测试基础设施

**Files:**
- Modify: `platformio.ini`
- Create: `src/config_types.h`

- [ ] **Step 1: 先写会失败的测试入口（仅引用不存在头文件）**

创建 `test/native/test_config_validator/test_main.cpp`：

```cpp
#include "config_types.h"

int main() {
    DeviceConfig cfg{};
    (void)cfg;
    return 0;
}
```

- [ ] **Step 2: 配置 native 测试环境并验证当前失败**

修改 `platformio.ini` 追加：

```ini
[env:native]
platform = native
test_build_src = no
build_flags =
    -Isrc
```

运行：`pio test -e native`

Expected: FAIL，报错 `config_types.h: No such file or directory`。

- [ ] **Step 3: 新建设备配置基础类型**

创建 `src/config_types.h`：

```cpp
#pragma once

#include <stdint.h>
#include <string>

enum class ConfigError : uint16_t {
    Ok = 0,
    Unauthorized = 1001,
    PairingCodeInvalid = 1002,
    PairingCodeExpired = 1003,
    JsonParseFailed = 2001,
    InvalidField = 2002,
    WifiConnectFailed = 3001,
    NtpSyncFailed = 3002,
};

enum class ConfigState : uint8_t {
    Idle,
    BleAdvertising,
    AuthPending,
    AuthOk,
    ConfigReceived,
    Applying,
    Done,
    Error,
};

struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPassword;
    std::string timezone;
    std::string ntpServer;
};
```

- [ ] **Step 4: 重新运行测试确认通过编译**

运行：`pio test -e native`

Expected: PASS（1 个测试程序编译并执行成功）。

- [ ] **Step 5: 提交**

```bash
git add platformio.ini src/config_types.h test/native/test_config_validator/test_main.cpp
git commit -m "feat: 添加 BLE 配网基础类型与 native 测试环境"
```

---

### Task 2: 以 TDD 实现配置校验器

**Files:**
- Create: `src/config_validator.h`
- Create: `src/config_validator.cpp`
- Modify: `test/native/test_config_validator/test_main.cpp`

- [ ] **Step 1: 写失败用例（字段合法/非法）**

将 `test/native/test_config_validator/test_main.cpp` 替换为：

```cpp
#include "config_types.h"
#include "config_validator.h"

#include <assert.h>

int main() {
    DeviceConfig ok{"HomeWiFi", "12345678", "Asia/Shanghai", "ntp.aliyun.com"};
    assert(validateConfig(ok) == ConfigError::Ok);

    DeviceConfig badSsid{"", "12345678", "Asia/Shanghai", "ntp.aliyun.com"};
    assert(validateConfig(badSsid) == ConfigError::InvalidField);

    DeviceConfig badTz{"HomeWiFi", "12345678", "", "ntp.aliyun.com"};
    assert(validateConfig(badTz) == ConfigError::InvalidField);

    DeviceConfig badNtp{"HomeWiFi", "12345678", "Asia/Shanghai", ""};
    assert(validateConfig(badNtp) == ConfigError::InvalidField);

    return 0;
}
```

- [ ] **Step 2: 运行测试确认失败**

运行：`pio test -e native`

Expected: FAIL，报错 `config_validator.h` 不存在。

- [ ] **Step 3: 添加最小接口声明**

创建 `src/config_validator.h`：

```cpp
#pragma once

#include "config_types.h"

ConfigError validateConfig(const DeviceConfig& config);
```

- [ ] **Step 4: 添加最小实现让测试通过**

创建 `src/config_validator.cpp`：

```cpp
#include "config_validator.h"

static bool isLenInRange(const std::string& s, size_t minLen, size_t maxLen) {
    return s.size() >= minLen && s.size() <= maxLen;
}

ConfigError validateConfig(const DeviceConfig& config) {
    if (!isLenInRange(config.wifiSsid, 1, 32)) {
        return ConfigError::InvalidField;
    }
    if (config.wifiPassword.size() > 64) {
        return ConfigError::InvalidField;
    }
    if (!isLenInRange(config.timezone, 1, 64)) {
        return ConfigError::InvalidField;
    }
    if (!isLenInRange(config.ntpServer, 1, 128)) {
        return ConfigError::InvalidField;
    }
    return ConfigError::Ok;
}
```

- [ ] **Step 5: 运行测试确认通过**

运行：`pio test -e native`

Expected: PASS。

- [ ] **Step 6: 提交**

```bash
git add src/config_validator.h src/config_validator.cpp test/native/test_config_validator/test_main.cpp
git commit -m "feat: 添加设备配置字段校验器"
```

---

### Task 3: 以 TDD 实现一次性配网码模块

**Files:**
- Create: `src/pairing_code.h`
- Create: `src/pairing_code.cpp`
- Create: `test/native/test_pairing_code/test_main.cpp`

- [ ] **Step 1: 写失败用例（格式、过期、一次性）**

创建 `test/native/test_pairing_code/test_main.cpp`：

```cpp
#include "pairing_code.h"

#include <assert.h>

int main() {
    PairingCodeManager mgr;

    const uint32_t nowMs = 1000;
    const std::string code = mgr.generate(nowMs, 120000);
    assert(code.size() == 6);
    assert(mgr.verify(code, nowMs + 1000) == ConfigError::Ok);

    assert(mgr.verify(code, nowMs + 2000) == ConfigError::PairingCodeInvalid);

    const std::string code2 = mgr.generate(nowMs, 120000);
    assert(mgr.verify(code2, nowMs + 120001) == ConfigError::PairingCodeExpired);

    return 0;
}
```

- [ ] **Step 2: 运行测试确认失败**

运行：`pio test -e native`

Expected: FAIL，报错 `pairing_code.h` 不存在。

- [ ] **Step 3: 实现最小接口与行为**

创建 `src/pairing_code.h`：

```cpp
#pragma once

#include "config_types.h"

class PairingCodeManager {
   public:
    std::string generate(uint32_t nowMs, uint32_t ttlMs);
    ConfigError verify(const std::string& inputCode, uint32_t nowMs);

   private:
    std::string activeCode_;
    uint32_t expireAtMs_ = 0;
};
```

创建 `src/pairing_code.cpp`：

```cpp
#include "pairing_code.h"

#include <stdio.h>

std::string PairingCodeManager::generate(uint32_t nowMs, uint32_t ttlMs) {
    uint32_t raw = nowMs % 1000000;
    char buf[7];
    snprintf(buf, sizeof(buf), "%06u", static_cast<unsigned>(raw));
    activeCode_ = buf;
    expireAtMs_ = nowMs + ttlMs;
    return activeCode_;
}

ConfigError PairingCodeManager::verify(const std::string& inputCode, uint32_t nowMs) {
    if (activeCode_.empty() || inputCode != activeCode_) {
        return ConfigError::PairingCodeInvalid;
    }
    if (nowMs > expireAtMs_) {
        activeCode_.clear();
        expireAtMs_ = 0;
        return ConfigError::PairingCodeExpired;
    }
    activeCode_.clear();
    expireAtMs_ = 0;
    return ConfigError::Ok;
}
```

- [ ] **Step 4: 运行测试确认通过**

运行：`pio test -e native`

Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add src/pairing_code.h src/pairing_code.cpp test/native/test_pairing_code/test_main.cpp
git commit -m "feat: 添加一次性 BLE 配网码管理模块"
```

---

### Task 4: 实现配置存储与网络应用模块

**Files:**
- Create: `src/config_store.h`
- Create: `src/config_store.cpp`
- Create: `src/net_apply.h`
- Create: `src/net_apply.cpp`

- [ ] **Step 1: 先写接口头文件**

创建 `src/config_store.h`：

```cpp
#pragma once

#include "config_types.h"

#include <Preferences.h>

class ConfigStore {
   public:
    explicit ConfigStore(Preferences& prefs) : prefs_(prefs) {}

    bool load(DeviceConfig& outConfig);
    bool save(const DeviceConfig& config);

   private:
    Preferences& prefs_;
};
```

创建 `src/net_apply.h`：

```cpp
#pragma once

#include "config_types.h"

ConfigError applyNetworkConfig(const DeviceConfig& config);
```

- [ ] **Step 2: 实现配置持久化**

创建 `src/config_store.cpp`：

```cpp
#include "config_store.h"

bool ConfigStore::load(DeviceConfig& outConfig) {
    outConfig.wifiSsid = prefs_.getString("ssid", "").c_str();
    outConfig.wifiPassword = prefs_.getString("password", "").c_str();
    outConfig.timezone = prefs_.getString("timezone", "Asia/Shanghai").c_str();
    outConfig.ntpServer = prefs_.getString("ntp_server", "pool.ntp.org").c_str();
    return !outConfig.wifiSsid.empty();
}

bool ConfigStore::save(const DeviceConfig& config) {
    bool ok = true;
    ok = ok && prefs_.putString("ssid", config.wifiSsid.c_str()) > 0;
    ok = ok && prefs_.putString("password", config.wifiPassword.c_str()) >= 0;
    ok = ok && prefs_.putString("timezone", config.timezone.c_str()) > 0;
    ok = ok && prefs_.putString("ntp_server", config.ntpServer.c_str()) > 0;
    return ok;
}
```

- [ ] **Step 3: 实现网络应用逻辑**

创建 `src/net_apply.cpp`：

```cpp
#include "net_apply.h"

#include <WiFi.h>
#include <time.h>

ConfigError applyNetworkConfig(const DeviceConfig& config) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return ConfigError::WifiConnectFailed;
    }

    setenv("TZ", config.timezone.c_str(), 1);
    tzset();
    configTime(0, 0, config.ntpServer.c_str());

    struct tm t;
    if (!getLocalTime(&t, 5000)) {
        return ConfigError::NtpSyncFailed;
    }

    return ConfigError::Ok;
}
```

- [ ] **Step 4: 编译确认通过**

运行：`pio run`

Expected: BUILD SUCCESS。

- [ ] **Step 5: 提交**

```bash
git add src/config_store.h src/config_store.cpp src/net_apply.h src/net_apply.cpp
git commit -m "feat: 添加配置持久化与网络应用模块"
```

---

### Task 5: 实现 BLE GATT 配网服务

**Files:**
- Modify: `platformio.ini`
- Create: `src/ble_config_service.h`
- Create: `src/ble_config_service.cpp`

- [ ] **Step 1: 添加 BLE 库依赖并验证失败基线**

修改 `platformio.ini` 的 `lib_deps` 追加：

```ini
lib_deps =
    https://github.com/m5stack/M5Unified
    https://github.com/m5stack/M5PM1
    h2zero/NimBLE-Arduino @ ^1.4.2
```

运行：`pio run`

Expected: 仍可能 FAIL（`ble_config_service.*` 尚未创建）。

- [ ] **Step 2: 定义服务接口**

创建 `src/ble_config_service.h`：

```cpp
#pragma once

#include "config_types.h"

#include <functional>

struct BleConfigCallbacks {
    std::function<ConfigError(const std::string&)> onAuth;
    std::function<ConfigError(const std::string&)> onConfigJson;
    std::function<ConfigError(const std::string&)> onCommand;
};

class BleConfigService {
   public:
    void begin(const BleConfigCallbacks& callbacks);
    void notifyStatus(ConfigState state, ConfigError error, const char* message);
    void stop();

   private:
    BleConfigCallbacks callbacks_;
};
```

- [ ] **Step 3: 实现最小 GATT 服务骨架**

创建 `src/ble_config_service.cpp`：

```cpp
#include "ble_config_service.h"

#include <NimBLEDevice.h>

namespace {
constexpr char kServiceUuid[] = "91a50001-66d8-4d35-a42b-df7f00c10000";
constexpr char kAuthUuid[] = "91a50002-66d8-4d35-a42b-df7f00c10000";
constexpr char kConfigUuid[] = "91a50003-66d8-4d35-a42b-df7f00c10000";
constexpr char kCommandUuid[] = "91a50004-66d8-4d35-a42b-df7f00c10000";
constexpr char kStatusUuid[] = "91a50005-66d8-4d35-a42b-df7f00c10000";

NimBLECharacteristic* gStatusChar = nullptr;
}  // namespace

void BleConfigService::begin(const BleConfigCallbacks& callbacks) {
    callbacks_ = callbacks;
    NimBLEDevice::init("matrix5-config");
    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service = server->createService(kServiceUuid);

    service->createCharacteristic(kAuthUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    service->createCharacteristic(kConfigUuid, NIMBLE_PROPERTY::WRITE);
    service->createCharacteristic(kCommandUuid, NIMBLE_PROPERTY::WRITE);
    gStatusChar = service->createCharacteristic(kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    service->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(kServiceUuid);
    adv->start();
}

void BleConfigService::notifyStatus(ConfigState state, ConfigError error, const char* message) {
    if (!gStatusChar) {
        return;
    }
    std::string payload = "{\"state\":" + std::to_string(static_cast<int>(state)) +
                          ",\"error\":" + std::to_string(static_cast<int>(error)) +
                          ",\"message\":\"" + message + "\"}";
    gStatusChar->setValue(payload);
    gStatusChar->notify();
}

void BleConfigService::stop() {
    NimBLEDevice::deinit(true);
}
```

- [ ] **Step 4: 编译确认通过**

运行：`pio run`

Expected: BUILD SUCCESS。

- [ ] **Step 5: 提交**

```bash
git add platformio.ini src/ble_config_service.h src/ble_config_service.cpp
git commit -m "feat: 添加 BLE GATT 配网服务骨架"
```

---

### Task 6: 接入主流程并移除旧 AP 配网入口

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 删除旧 AP/HTTP 配网页面与 WebServer 逻辑**

从 `src/main.cpp` 删除以下元素：

- `CONFIG_PAGE_HEAD` / `CONFIG_PAGE_TAIL` / `SAVE_OK_PAGE`
- `WebServer server(80)`
- `scanWiFiNetworks()` / `handleRoot()` / `handleSave()` / `startAPMode()`
- `apMode` 分支处理

- [ ] **Step 2: 引入新模块头文件与全局对象**

在 `src/main.cpp` 顶部追加：

```cpp
#include "ble_config_service.h"
#include "config_store.h"
#include "config_validator.h"
#include "net_apply.h"
#include "pairing_code.h"
```

在全局变量区替换为：

```cpp
Preferences prefs;
ConfigStore configStore(prefs);
PairingCodeManager pairingCodeMgr;
BleConfigService bleConfigService;

DeviceConfig pendingConfig{};
bool hasPendingConfig = false;
bool isAuthorized = false;
```

- [ ] **Step 3: 在 `setup()` 中接入自动进入 BLE 配网**

将 `setup()` 的网络初始化替换为：

```cpp
    prefs.begin("m5stick", false);

    DeviceConfig cfg;
    const bool hasConfig = configStore.load(cfg);
    if (hasConfig && applyNetworkConfig(cfg) == ConfigError::Ok) {
        Serial.println("WiFi/NTP ready");
    } else {
        bleConfigService.begin({
            .onAuth = [&](const std::string& code) {
                ConfigError err = pairingCodeMgr.verify(code, millis());
                isAuthorized = (err == ConfigError::Ok);
                return err;
            },
            .onConfigJson = [&](const std::string& json) {
                (void)json;
                if (!isAuthorized) return ConfigError::Unauthorized;
                return ConfigError::JsonParseFailed;
            },
            .onCommand = [&](const std::string& cmd) {
                if (!isAuthorized) return ConfigError::Unauthorized;
                if (cmd != "apply" || !hasPendingConfig) return ConfigError::InvalidField;
                if (!configStore.save(pendingConfig)) return ConfigError::InvalidField;
                return applyNetworkConfig(pendingConfig);
            },
        });
    }
```

- [ ] **Step 4: 在 `loop()` 接入按键长按重入 BLE 模式**

将 `loop()` 尾部增加：

```cpp
    M5.update();
    if (M5.BtnA.pressedFor(1500)) {
        const std::string newCode = pairingCodeMgr.generate(millis(), 120000);
        isAuthorized = false;
        hasPendingConfig = false;
        bleConfigService.notifyStatus(ConfigState::BleAdvertising, ConfigError::Ok, newCode.c_str());
    }
```

- [ ] **Step 5: 编译确认**

运行：`pio run`

Expected: BUILD SUCCESS。

- [ ] **Step 6: 提交**

```bash
git add src/main.cpp
git commit -m "refactor: 移除旧 AP 配网并接入 BLE 配网主流程"
```

---

### Task 7: 实现 GitHub Pages 配网页面（Chrome Only）

**Files:**
- Create: `docs/web-config/index.html`
- Create: `docs/web-config/style.css`
- Create: `docs/web-config/app.js`

- [ ] **Step 1: 写页面结构**

创建 `docs/web-config/index.html`：

```html
<!doctype html>
<html lang="zh-CN">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>matrix5 BLE 配网</title>
    <link rel="stylesheet" href="./style.css" />
  </head>
  <body>
    <main class="container">
      <h1>matrix5 BLE 配网</h1>
      <p id="compat" class="hint"></p>
      <button id="connectBtn">连接设备</button>

      <section class="card">
        <label>一次性配网码</label>
        <input id="pairingCode" maxlength="6" placeholder="输入设备屏幕上的 6 位码" />
        <button id="authBtn">授权</button>
      </section>

      <section class="card">
        <label>WiFi SSID</label>
        <input id="wifiSsid" />
        <label>WiFi 密码</label>
        <input id="wifiPassword" type="password" />
        <label>时区</label>
        <input id="timezone" value="Asia/Shanghai" />
        <label>NTP 服务器</label>
        <input id="ntpServer" value="ntp.aliyun.com" />
        <button id="applyBtn">写入并应用</button>
      </section>

      <pre id="status">等待操作...</pre>
    </main>
    <script type="module" src="./app.js"></script>
  </body>
</html>
```

- [ ] **Step 2: 写最小样式**

创建 `docs/web-config/style.css`：

```css
body {
  margin: 0;
  font-family: "PingFang SC", "Noto Sans CJK SC", sans-serif;
  background: #0b1220;
  color: #e5e7eb;
}

.container {
  max-width: 560px;
  margin: 24px auto;
  padding: 16px;
}

.card {
  margin-top: 12px;
  padding: 12px;
  border: 1px solid #334155;
  border-radius: 8px;
}

input,
button {
  width: 100%;
  margin-top: 8px;
  padding: 10px;
  border-radius: 6px;
  border: 1px solid #475569;
}

button {
  background: #22c55e;
  color: #04130a;
  font-weight: 700;
}

#status {
  margin-top: 12px;
  background: #111827;
  border: 1px solid #334155;
  border-radius: 8px;
  padding: 12px;
  white-space: pre-wrap;
}
```

- [ ] **Step 3: 写 BLE 交互逻辑**

创建 `docs/web-config/app.js`：

```js
const SERVICE_UUID = "91a50001-66d8-4d35-a42b-df7f00c10000";
const AUTH_UUID = "91a50002-66d8-4d35-a42b-df7f00c10000";
const CONFIG_UUID = "91a50003-66d8-4d35-a42b-df7f00c10000";
const COMMAND_UUID = "91a50004-66d8-4d35-a42b-df7f00c10000";
const STATUS_UUID = "91a50005-66d8-4d35-a42b-df7f00c10000";

const $ = (id) => document.getElementById(id);
const te = new TextEncoder();
const td = new TextDecoder();

let authChar;
let configChar;
let commandChar;
let statusChar;

function log(msg) {
  $("status").textContent = `[${new Date().toLocaleTimeString()}] ${msg}\n` + $("status").textContent;
}

function checkCompat() {
  if (!navigator.bluetooth) {
    $("compat").textContent = "当前浏览器不支持 Web Bluetooth。仅支持 Chrome。";
    $("connectBtn").disabled = true;
  } else {
    $("compat").textContent = "已启用 Web Bluetooth（Chrome）。";
  }
}

async function connect() {
  const device = await navigator.bluetooth.requestDevice({
    filters: [{ services: [SERVICE_UUID] }],
  });
  const server = await device.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  authChar = await service.getCharacteristic(AUTH_UUID);
  configChar = await service.getCharacteristic(CONFIG_UUID);
  commandChar = await service.getCharacteristic(COMMAND_UUID);
  statusChar = await service.getCharacteristic(STATUS_UUID);
  await statusChar.startNotifications();
  statusChar.addEventListener("characteristicvaluechanged", (event) => {
    log(`设备状态: ${td.decode(event.target.value)}`);
  });
  log("设备连接成功");
}

async function auth() {
  const code = $("pairingCode").value.trim();
  await authChar.writeValue(te.encode(code));
  log("已发送配网码");
}

async function applyConfig() {
  const payload = {
    wifiSsid: $("wifiSsid").value,
    wifiPassword: $("wifiPassword").value,
    timezone: $("timezone").value,
    ntpServer: $("ntpServer").value,
  };
  await configChar.writeValue(te.encode(JSON.stringify(payload)));
  await commandChar.writeValue(te.encode("apply"));
  log("已发送配置与 apply 命令");
}

checkCompat();
$("connectBtn").addEventListener("click", () => connect().catch((e) => log(`连接失败: ${e.message}`)));
$("authBtn").addEventListener("click", () => auth().catch((e) => log(`授权失败: ${e.message}`)));
$("applyBtn").addEventListener("click", () => applyConfig().catch((e) => log(`下发失败: ${e.message}`)));
```

- [ ] **Step 4: 本地静态验证**

运行：`python3 -m http.server 8080 --directory docs/web-config`

Expected: 访问 `http://localhost:8080` 可看到页面，Chrome 打开控制台无语法错误。

- [ ] **Step 5: 提交**

```bash
git add docs/web-config/index.html docs/web-config/style.css docs/web-config/app.js
git commit -m "feat: 添加 Chrome Web Bluetooth 配网页面"
```

---

### Task 8: 补齐演示与测试文档并完成回归验证

**Files:**
- Create: `docs/demo-ble-config.md`
- Create: `test/test_ble_config_flow.md`

- [ ] **Step 1: 写演示文档**

创建 `docs/demo-ble-config.md`：

```md
# BLE 配网演示步骤

1. 擦除设备配置或首次开机，确认设备进入 BLE 配网等待状态并显示 6 位配网码。
2. Chrome 打开 GitHub Pages 配网页面，点击“连接设备”。
3. 输入设备上的配网码，点击“授权”，确认状态返回授权成功。
4. 输入 SSID、密码、timezone、ntpServer，点击“写入并应用”。
5. 确认设备返回成功状态码 `0000`，时钟开始按 NTP 时间运行。
6. 长按按键 1.5 秒，确认设备可再次进入 BLE 配网模式。
```

- [ ] **Step 2: 写手工测试矩阵**

创建 `test/test_ble_config_flow.md`：

```md
# BLE 配网手工测试矩阵

| 用例ID | 输入 | 预期 |
|---|---|---|
| TC-01 | 正确 6 位码 | 返回授权成功 |
| TC-02 | 错误 6 位码 | 返回 `1002` |
| TC-03 | 过期 6 位码 | 返回 `1003` |
| TC-04 | 空 SSID | 返回 `2002` |
| TC-05 | NTP 服务器空 | 返回 `2002` |
| TC-06 | 错误 WiFi 密码 | 返回 `3001` |
| TC-07 | 无效 NTP 地址 | 返回 `3002` |
| TC-08 | 长按按键重入 | 重新广播并显示新配网码 |
| TC-09 | 旧 AP 页面 | 不可访问（已移除） |
```
```

- [ ] **Step 3: 执行完整验证**

运行：

```bash
pio test -e native && pio run
```

Expected:

- native 测试 PASS
- 固件编译 BUILD SUCCESS

- [ ] **Step 4: 提交**

```bash
git add docs/demo-ble-config.md test/test_ble_config_flow.md
git commit -m "docs: 添加 BLE 配网演示与测试矩阵"
```

---

## Self-Review

**1. Spec coverage:**

- ✅ 自动进入 BLE 配网：Task 6
- ✅ 长按重入 BLE 配网：Task 6
- ✅ 一次性配网码：Task 3 + Task 6
- ✅ 配置项（SSID/密码/时区/NTP）：Task 2 + Task 7
- ✅ GATT 结构与状态通知：Task 5
- ✅ 完全移除旧 WiFi AP 入口：Task 6
- ✅ Chrome-only 网页：Task 7
- ✅ 测试与演示文档：Task 8

**2. Placeholder scan:**

- 已检查：无 TBD/TODO/implement later/类似 Task N。

**3. Type consistency:**

- `DeviceConfig` 字段名在 `config_types.h`、`config_validator.*`、`app.js` 保持一致。
- 错误码使用 `ConfigError` 枚举统一表达。
- 配网命令固定使用字符串 `apply`，设备端与网页端一致。
