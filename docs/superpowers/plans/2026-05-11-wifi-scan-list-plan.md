# Wi-Fi 扫描列表功能实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Web 配置网络页面中，自动扫描并显示周围可用的 2.4GHz Wi-Fi 网络列表（最多 5 个，按信号强度排序，显示 dBm），用户点击 SSID 后自动填入 Wi-Fi 名称输入框。

**Architecture:** 复用现有 BLE `Command` + `Status` 特性传输扫描结果。固件端通过 `WiFi.scanNetworks(true)` 异步扫描，在 `loop()` 中轮询结果后格式化为 JSON 数组通过 Status notify 发送。Web 端进入配置页面后自动发送 `"scan_wifi"` 命令，收到 `state=9` 响应后渲染可点击的 SSID 列表。

**Tech Stack:** ESP32-S3 + Arduino + NimBLE-Arduino + WiFi.h（固件）；Web Bluetooth API + 原生 JavaScript（Web）

---

## 文件结构映射

### 固件端（main 分支）

| 文件 | 职责 | 改动类型 |
|------|------|---------|
| `src/config_types.h` | 定义 ConfigState 和 ConfigError 枚举 | 新增 `ScanComplete = 9` 和 `WifiScanFailed = 3003` |
| `src/main.cpp` | 主程序：包含 BLE 回调、loop()、Wi-Fi 扫描逻辑 | 新增扫描状态变量、`"scan_wifi"` 命令处理、异步扫描逻辑、结果格式化函数 |

### Web 端（gh-pages 分支）

| 文件 | 职责 | 改动类型 |
|------|------|---------|
| `index.html` | 配置网络页面结构 | 在 Wi-Fi 名称输入框下方新增 `#wifiScanStatus` 和 `#wifiScanList` |
| `style.css` | 页面样式 | 新增扫描列表相关样式（`.scan-status`、`.scan-list`、`.scan-item` 等） |
| `app.js` | 页面逻辑 | `showConfigView()` 自动触发扫描；`handleStatus` 新增 `state === 9` 处理；新增 `renderWifiScanList()` 函数 |

---

### Task 1: 扩展配置类型枚举（固件端）

**分支:** `main`

**Files:**
- Modify: `src/config_types.h`

- [ ] **Step 1: 新增 ConfigError::WifiScanFailed**

在 `src/config_types.h` 的 `ConfigError` 枚举中，在 `NtpSyncFailed = 3002` 下方添加：

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
    WifiScanFailed = 3003,
};
```

- [ ] **Step 2: 新增 ConfigState::ScanComplete**

在 `src/config_types.h` 的 `ConfigState` 枚举中，在 `PairedDeviceConnected` 下方添加：

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
    ScanComplete = 9,
};
```

- [ ] **Step 3: 编译验证**

Run: `pio run -e m5stick-s3`
Expected: 编译通过（此时 main.cpp 还未修改，但枚举新增不会破坏现有代码）

- [ ] **Step 4: 提交**

```bash
git add src/config_types.h
git commit -m "feat(config_types): add ScanComplete state and WifiScanFailed error"
```

---

### Task 2: 固件端 Wi-Fi 扫描逻辑（main.cpp）

**分支:** `main`

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 在全局区新增 Wi-Fi 扫描状态变量**

在 `src/main.cpp` 第 60 行（`bool applyRequested = false;`）下方添加：

```cpp
bool wifiScanRequested = false;
bool wifiScanInProgress = false;
```

- [ ] **Step 2: 在 onCommand 回调中添加 `"scan_wifi"` 处理**

在 `src/main.cpp` 第 224-234 行的 `callbacks.onCommand` lambda 中，在 `"clear_paired"` 处理之后、`"apply"` 检查之前添加：

```cpp
        callbacks.onCommand = [&](const std::string& cmd) {
            if (!isAuthorized) return ConfigError::Unauthorized;
            if (cmd == "clear_paired") {
                pairedDeviceStore.clear();
                prefs.putString("paired_devices", "");
                bleConfigService.notifyStatus(ConfigState::BleAdvertising, ConfigError::Ok, "cleared");
                return ConfigError::Ok;
            }
            if (cmd == "scan_wifi") {
                wifiScanRequested = true;
                return ConfigError::Ok;
            }
            if (cmd != "apply" || !hasPendingConfig) return ConfigError::InvalidField;
            applyRequested = true;
            return ConfigError::Ok;
        };
```

- [ ] **Step 3: 在 loop() 中添加异步 Wi-Fi 扫描逻辑**

在 `src/main.cpp` 第 321-344 行的 `if (applyRequested)` 块之后、`M5.delay(10);` 之前添加：

```cpp
    // Wi-Fi async scan
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
                bleConfigService.notifyStatus(ConfigState::ScanComplete, ConfigError::Ok, "[]");
            } else {
                std::string json = buildScanResultJson(n);
                bleConfigService.notifyStatus(ConfigState::ScanComplete, ConfigError::Ok, json.c_str());
            }
            WiFi.scanDelete();
        } else if (n == -2) {
            wifiScanInProgress = false;
            bleConfigService.notifyStatus(ConfigState::ScanComplete, ConfigError::WifiScanFailed, "scan_failed");
        }
    }
```

- [ ] **Step 4: 添加 JSON 转义辅助函数**

在 `src/main.cpp` 中 `drawPairedConfirmScreen()` 函数之后、`enterBleConfigMode()` 函数之前添加：

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

- [ ] **Step 5: 添加扫描结果格式化函数**

在 `escapeJsonString()` 函数之后、`enterBleConfigMode()` 函数之前添加：

```cpp
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
```

- [ ] **Step 6: 添加必要的头文件**

在 `src/main.cpp` 顶部确认已有 `#include <WiFi.h>` 和 `#include <vector>` 和 `#include <algorithm>`。如果没有 `<vector>` 和 `<algorithm>`，在第 6 行（`#include <cstring>`）下方添加：

```cpp
#include <vector>
#include <algorithm>
```

- [ ] **Step 7: 编译验证**

Run: `pio run -e m5stick-s3`
Expected: 编译通过，无错误

- [ ] **Step 8: 提交**

```bash
git add src/main.cpp
git commit -m "feat(main): add async Wi-Fi scan command and result formatting"
```

---

### Task 3: Web 端 HTML 结构修改

**分支:** `gh-pages`

**Files:**
- Modify: `index.html`

- [ ] **Step 1: 切换到 gh-pages 分支**

```bash
git checkout gh-pages
```

- [ ] **Step 2: 在 Wi-Fi 名称输入框下方新增扫描区域**

在 `index.html` 第 57-58 行（`<label>WiFi 名称</label>` 和 `<input id="wifiSsid" placeholder="SSID" />`）之后添加：

```html
          <div id="wifiScanStatus" class="scan-status" style="display:none;"></div>
          <div id="wifiScanList" class="scan-list"></div>
```

完整修改后的区域应该是：

```html
          <label>WiFi 名称</label>
          <input id="wifiSsid" placeholder="SSID" />
          <div id="wifiScanStatus" class="scan-status" style="display:none;"></div>
          <div id="wifiScanList" class="scan-list"></div>
          <label>WiFi 密码</label>
```

- [ ] **Step 3: 验证 HTML 结构**

打开 `index.html` 检查 `#wifiScanStatus` 和 `#wifiScanList` 是否正确地位于 Wi-Fi 名称输入框和密码输入框之间。

- [ ] **Step 4: 提交**

```bash
git add index.html
git commit -m "feat(html): add Wi-Fi scan status and list containers"
```

---

### Task 4: Web 端 CSS 样式添加

**分支:** `gh-pages`

**Files:**
- Modify: `style.css`

- [ ] **Step 1: 在 style.css 末尾添加扫描列表样式**

在 `style.css` 文件末尾（`empty-state .sub` 规则之后）添加：

```css
/* ---- Wi-Fi scan list ---- */
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

- [ ] **Step 2: 提交**

```bash
git add style.css
git commit -m "feat(css): add Wi-Fi scan list styles"
```

---

### Task 5: Web 端 JavaScript 逻辑修改

**分支:** `gh-pages`

**Files:**
- Modify: `app.js`

- [ ] **Step 1: 修改 showConfigView() 自动触发扫描**

将 `app.js` 中的 `showConfigView()` 函数：

```javascript
function showConfigView() {
  $("configStatus").textContent = "";
  $("configStatus").className = "step-status";
  showView("config");
}
```

修改为：

```javascript
function showConfigView() {
  $("configStatus").textContent = "";
  $("configStatus").className = "step-status";
  $("wifiScanStatus").textContent = "正在扫描附近 Wi-Fi...";
  $("wifiScanStatus").style.display = "block";
  $("wifiScanList").innerHTML = "";
  showView("config");
  if (commandChar && isAuthorized) {
    commandChar.writeValue(te.encode("scan_wifi")).catch((e) => {
      $("wifiScanStatus").textContent = "扫描请求失败: " + e.message;
    });
  }
}
```

- [ ] **Step 2: 在 handleStatus 中新增 state === 9 的处理**

在 `app.js` 的 `connectBle()` 函数中的 `handleStatus` 里，在 `s.state === 6` 的处理分支之后、`s.error !== 0` 的检查之前添加：

```javascript
      if (s.state === 8) {
        // PairedDeviceConnected
        if (!deleteFlowTarget) {
          setConnectStatus("ok", "已配对设备，自动授权");
          addOrUpdateDevice({ id: device.id, name: device.name || "matrix5" });
          setTimeout(() => showConfigView(), 300);
        }
      } else if (s.state === 3) {
        // AuthOk
        setConnectStatus("ok", "授权成功");
        addOrUpdateDevice({ id: device.id, name: device.name || "matrix5" });
        setTimeout(() => showConfigView(), 300);
      } else if (s.state === 9) {
        // ScanComplete
        if (s.error !== 0) {
          $("wifiScanStatus").textContent = "扫描失败，请手动输入";
          $("wifiScanList").innerHTML = "";
        } else {
          try {
            const networks = JSON.parse(s.message);
            renderWifiScanList(networks);
          } catch {
            $("wifiScanStatus").textContent = "扫描结果解析失败";
            $("wifiScanList").innerHTML = "";
          }
        }
      } else if (s.state === 6) {
        // Done
        $("configStatus").className = "step-status ok";
        $("configStatus").textContent = "配置已应用成功";
        setTimeout(() => {
          showView("deviceList");
          renderDeviceList();
          resetConnection();
        }, 1500);
      } else if (s.error !== 0) {
        setConnectStatus("err", `错误码 ${s.error}: ${s.message}`);
      }
```

- [ ] **Step 3: 新增 renderWifiScanList() 函数**

在 `app.js` 中 `showConfigView()` 函数之前（或文件末尾的初始化代码之前）添加：

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
```

- [ ] **Step 4: 验证代码完整性**

检查以下内容：
1. `showConfigView()` 已修改，进入配置页后自动发送 `"scan_wifi"` 命令
2. `handleStatus` 已新增 `s.state === 9` 分支
3. `renderWifiScanList()` 函数已定义
4. `escapeHtml()` 函数已在文件中定义，可被 `renderWifiScanList()` 复用

- [ ] **Step 5: 提交**

```bash
git add app.js
git commit -m "feat(app): auto-scan Wi-Fi on config page and render selectable SSID list"
```

---

## 自检清单

**1. Spec 覆盖检查：**

| 设计需求 | 对应任务 |
|---------|---------|
| ConfigState::ScanComplete = 9 | Task 1 Step 2 |
| ConfigError::WifiScanFailed = 3003 | Task 1 Step 1 |
| `"scan_wifi"` 命令处理 | Task 2 Step 2 |
| 异步 Wi-Fi 扫描（不阻塞 BLE） | Task 2 Step 3 |
| 去重、排序、最多 5 个 | Task 2 Step 5 |
| 显示 dBm 数值 | Task 5 Step 3（`renderWifiScanList` 的 rssi 显示） |
| Web 自动触发扫描 | Task 5 Step 1 |
| state=9 状态处理 | Task 5 Step 2 |
| 点击 SSID 填入输入框 | Task 5 Step 3 |
| 扫描失败/无网络/解析失败处理 | Task 5 Step 2、Step 3 |

**2. Placeholder 扫描：** 无 TBD、TODO、"implement later"、"add appropriate error handling" 等占位符。所有步骤包含完整代码。

**3. 类型一致性检查：**
- `ConfigState::ScanComplete` 值为 `9`，与 `app.js` 中 `s.state === 9` 一致
- `ConfigError::WifiScanFailed` 值为 `3003`，与 `app.js` 中 `s.error !== 0` 通用处理兼容
- `"scan_wifi"` 命令字符串在固件端（Task 2 Step 2）和 Web 端（Task 5 Step 1）一致
- `buildScanResultJson()` 生成的 JSON 格式 `[{"ssid":"...","rssi":-45},...]` 与 Web 端 `JSON.parse(s.message)` 兼容

---

## 实施完成后验证

**固件端验证：**
1. 编译通过：`pio run -e m5stick-s3`
2. 烧录到设备后，进入 BLE 配置模式
3. 通过 Web 连接设备并授权
4. 进入配置页面后，观察设备日志确认 `WiFi.scanNetworks(true)` 被调用
5. 观察 Web 端是否正确显示扫描到的 Wi-Fi 列表

**Web 端验证：**
1. 在 Chrome/Edge 中打开页面
2. 添加/连接设备，完成授权
3. 进入配置页面后，观察是否显示"正在扫描附近 Wi-Fi..."
4. 等待几秒后，观察是否显示 Wi-Fi 列表（最多 5 个，按信号强度排序，带 dBm）
5. 点击某个 SSID，观察是否自动填入 Wi-Fi 名称输入框
6. 点击"写入并应用"，确认配置流程正常完成

**边界情况验证：**
- 无 Wi-Fi 环境：应显示"未找到可用 Wi-Fi 网络"
- 扫描失败：应显示"扫描失败，请手动输入"
- 多个相同 SSID：列表中只保留信号最强的一个
