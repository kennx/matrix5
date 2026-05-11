# Wi-Fi 扫描列表功能设计文档

## 目标
在 Web 配置网络页面中，自动扫描并显示周围可用的 2.4GHz Wi-Fi 网络列表，用户点击 SSID 后自动填入 Wi-Fi 名称输入框。

## 架构
复用现有 BLE `Command` + `Status` 特性传输扫描结果，无需新增 GATT 特性。Web 端进入配置页面后自动触发扫描，设备通过异步 `WiFi.scanNetworks(true)` 完成扫描后返回 JSON 数组。

## 技术栈
- 固件：ESP32-S3 + Arduino + NimBLE-Arduino + WiFi.h
- Web：Web Bluetooth API + 原生 JavaScript

---

## 协议扩展

### 新增 ConfigState

```cpp
enum class ConfigState : uint8_t {
    Idle,
    BleAdvertising,
    AuthPending,
    AuthOk,
    ConfigReceived,
    Applying,
    Done,
    Error,
    PairedDeviceConnected,
    ScanComplete = 9,   // 新增：Wi-Fi 扫描完成
};
```

### 新增 ConfigError

```cpp
enum class ConfigError : uint16_t {
    Ok = 0,
    Unauthorized = 1001,
    PairingCodeInvalid = 1002,
    PairingCodeExpired = 1003,
    JsonParseFailed = 2001,
    InvalidField = 2002,
    WifiConnectFailed = 3001,
    NtpSyncFailed = 3002,
    WifiScanFailed = 3003,   // 新增：Wi-Fi 扫描失败
};
```

### 命令与响应格式

**Web → Device（Command 特性）**

```
scan_wifi
```

**Device → Web（Status notify）**

扫描成功：
```json
{"state":9,"error":0,"message":"[{\"ssid\":\"MyWiFi\",\"rssi\":-45},{\"ssid\":\"Guest\",\"rssi\":-62}]"}
```

无网络：
```json
{"state":9,"error":0,"message":"[]"}
```

扫描失败：
```json
{"state":9,"error":3003,"message":"scan_failed"}
```

未授权：
```json
{"state":7,"error":1001,"message":"unauthorized"}
```

---

## 固件端设计（main 分支）

### 扫描状态变量

在 `main.cpp` 全局区新增：

```cpp
bool wifiScanRequested = false;   // 收到扫描命令
bool wifiScanInProgress = false;  // 扫描进行中
```

### onCommand 处理

在 `callbacks.onCommand` lambda 中添加：

```cpp
if (cmd == "scan_wifi") {
    wifiScanRequested = true;
    return ConfigError::Ok;
}
```

返回 Ok 后立即释放 BLE 回调，不阻塞。

### loop() 异步扫描逻辑

在 `loop()` 函数中添加（放在 `M5.delay(10)` 之前）：

```cpp
// Wi-Fi 异步扫描
if (wifiScanRequested && !wifiScanInProgress) {
    wifiScanRequested = false;
    wifiScanInProgress = true;
    WiFi.scanNetworks(true);  // true = 异步模式
}

if (wifiScanInProgress) {
    int n = WiFi.scanComplete();
    if (n == -1) {
        // 扫描仍在进行中，不做任何事
    } else if (n >= 0) {
        wifiScanInProgress = false;
        if (n == 0) {
            bleConfigService.notifyStatus(
                ConfigState::ScanComplete, ConfigError::Ok, "[]");
        } else {
            // 收集结果 → 去重 → 排序 → 取前 5 → 格式化为 JSON
            std::string json = buildScanResultJson(n);
            bleConfigService.notifyStatus(
                ConfigState::ScanComplete, ConfigError::Ok, json.c_str());
        }
        WiFi.scanDelete();
    } else {
        // n == -2 或其他错误
        wifiScanInProgress = false;
        bleConfigService.notifyStatus(
            ConfigState::ScanComplete, ConfigError::WifiScanFailed, "scan_failed");
    }
}
```

### 扫描结果处理函数

```cpp
static std::string buildScanResultJson(int scanCount) {
    struct Network { std::string ssid; int rssi; };
    std::vector<Network> networks;

    for (int i = 0; i < scanCount; i++) {
        std::string ssid = WiFi.SSID(i).c_str();
        int rssi = WiFi.RSSI(i);
        if (ssid.empty()) continue;
        // 去重：相同 SSID 只保留信号最强的
        auto it = std::find_if(networks.begin(), networks.end(),
            [&](const Network& n) { return n.ssid == ssid; });
        if (it != networks.end()) {
            if (rssi > it->rssi) it->rssi = rssi;
        } else {
            networks.push_back({ssid, rssi});
        }
    }

    // 按信号强度从高到低排序
    std::sort(networks.begin(), networks.end(),
        [](const Network& a, const Network& b) { return a.rssi > b.rssi; });

    // 最多取 5 个
    if (networks.size() > 5) networks.resize(5);

    // 格式化为 JSON
    std::string json = "[";
    for (size_t i = 0; i < networks.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + escapeJsonString(networks[i].ssid) + "\",\"rssi\":"
              + std::to_string(networks[i].rssi) + "}";
    }
    json += "]";
    return json;
}
```

### JSON 转义辅助函数

```cpp
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
```

---

## Web 端设计（gh-pages 分支）

### HTML 结构修改

在 `index.html` 的 Wi-Fi 名称输入框下方新增扫描列表区域：

```html
<label>WiFi 名称</label>
<input id="wifiSsid" placeholder="SSID" />
<div id="wifiScanStatus" class="scan-status">正在扫描附近 Wi-Fi...</div>
<div id="wifiScanList" class="scan-list"></div>
```

### CSS 新增样式

```css
.scan-status {
  margin-top: 8px;
  font-size: 13px;
  color: var(--muted);
  min-height: 20px;
}

.scan-list {
  margin-top: 8px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.scan-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 12px;
  background: #1e293b;
  border: 1px solid var(--border-light);
  border-radius: 8px;
  cursor: pointer;
  transition: border-color .15s, background .15s;
}

.scan-item:hover {
  border-color: var(--accent);
  background: #162032;
}

.scan-item.selected {
  border-color: var(--accent);
  background: #0f2a1a;
}

.scan-item .ssid {
  font-size: 14px;
  font-weight: 500;
}

.scan-item .rssi {
  font-size: 12px;
  color: var(--muted);
}
```

### JavaScript 逻辑修改

**1. 进入配置页面后自动扫描**

修改 `showConfigView()`：

```javascript
function showConfigView() {
  $("configStatus").textContent = "";
  $("configStatus").className = "step-status";
  $("wifiScanStatus").textContent = "正在扫描附近 Wi-Fi...";
  $("wifiScanStatus").style.display = "block";
  $("wifiScanList").innerHTML = "";
  showView("config");
  // 自动触发扫描
  if (commandChar && isAuthorized) {
    commandChar.writeValue(te.encode("scan_wifi")).catch(() => {});
  }
}
```

**2. 新增 handleStatus 中 state=9 的处理**

在 `connectBle()` 的 `handleStatus` 中添加：

```javascript
} else if (s.state === 9) {
  // ScanComplete
  if (s.error !== 0) {
    $("wifiScanStatus").textContent = "扫描失败，请手动输入";
  } else {
    try {
      const networks = JSON.parse(s.message);
      renderWifiScanList(networks);
    } catch {
      $("wifiScanStatus").textContent = "扫描结果解析失败";
    }
  }
}
```

**3. 渲染扫描列表**

```javascript
function renderWifiScanList(networks) {
  const listEl = $("wifiScanList");
  const statusEl = $("wifiScanStatus");

  if (networks.length === 0) {
    statusEl.textContent = "未找到可用 Wi-Fi 网络";
    listEl.innerHTML = "";
    return;
  }

  statusEl.textContent = "点击选择要连接的网络：";
  listEl.innerHTML = networks.map((n, idx) => `
    <div class="scan-item" data-ssid="${escapeHtml(n.ssid)}" data-idx="${idx}">
      <span class="ssid">${escapeHtml(n.ssid)}</span>
      <span class="rssi">${n.rssi} dBm</span>
    </div>
  `).join("");

  listEl.querySelectorAll(".scan-item").forEach((item) => {
    item.addEventListener("click", (e) => {
      const ssid = e.currentTarget.dataset.ssid;
      $("wifiSsid").value = ssid;
      // 高亮选中的项
      listEl.querySelectorAll(".scan-item").forEach((el) => el.classList.remove("selected"));
      e.currentTarget.classList.add("selected");
    });
  });
}
```

### 错误处理

| 场景 | Web 端显示 |
|------|-----------|
| 扫描成功，有网络 | `点击选择要连接的网络：` + 5 个卡片 |
| 扫描成功，无网络 | `未找到可用 Wi-Fi 网络` |
| 扫描失败 | `扫描失败，请手动输入` |
| 进入配置页时未连接 | 不显示扫描区域（保持空白） |

---

## 改动文件清单

### 固件端（main 分支）

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `src/config_types.h` | 修改 | ConfigState 新增 `ScanComplete = 9`，ConfigError 新增 `WifiScanFailed = 3003` |
| `src/main.cpp` | 修改 | 新增 `wifiScanRequested`、`wifiScanInProgress` 标志；onCommand 添加 `"scan_wifi"` 处理；loop() 添加异步扫描逻辑；新增 `buildScanResultJson()` 和 `escapeJsonString()` 辅助函数 |

### Web 端（gh-pages 分支）

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `index.html` | 修改 | Wi-Fi 名称输入框下方新增 `#wifiScanStatus` 和 `#wifiScanList` 容器 |
| `style.css` | 修改 | 新增 `.scan-status`、`.scan-list`、`.scan-item` 等样式 |
| `app.js` | 修改 | `showConfigView()` 自动发送扫描命令；`handleStatus` 新增 `state === 9` 处理；新增 `renderWifiScanList()` 函数 |
