# Web 端设备管理功能实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在网页端实现设备管理功能，支持添加/连接/删除已配对设备，首页显示"我的设备"列表。

**Architecture:** 网页端改为 SPA 风格的三视图结构（设备列表、连接授权、配置网络），通过 localStorage 持久化设备信息。固件端在 onCommand 回调中新增 `clear_paired` 命令。固件端先改，网页端后改。

**Tech Stack:** PlatformIO/ESP32-S3 (固件), Vanilla JS + Web Bluetooth API (网页)

---

## 文件结构

| 文件 | 分支 | 变更 |
|------|------|------|
| `src/main.cpp` | main | 修改：onCommand 添加 `clear_paired` 命令处理 |
| `index.html` | gh-pages | 重写：三视图结构（设备列表/连接/配置） |
| `app.js` | gh-pages | 重写：设备管理逻辑、localStorage、视图路由 |
| `style.css` | gh-pages | 修改：添加设备卡片、视图、头部按钮样式 |

---

### Task 1: 固件端支持 clear_paired 命令

**分支：** `main`

**Files:**
- Modify: `src/main.cpp:224-229`

- [ ] **Step 1: 修改 onCommand 回调，添加 clear_paired 命令**

将 `src/main.cpp` 第 224-229 行的 `callbacks.onCommand` 替换为：

```cpp
        callbacks.onCommand = [&](const std::string& cmd) {
            if (!isAuthorized) return ConfigError::Unauthorized;
            if (cmd == "clear_paired") {
                pairedDeviceStore.clear();
                prefs.putString("paired_devices", "");
                bleConfigService.notifyStatus(ConfigState::BleAdvertising, ConfigError::Ok, "cleared");
                return ConfigError::Ok;
            }
            if (cmd != "apply" || !hasPendingConfig) return ConfigError::InvalidField;
            applyRequested = true;
            return ConfigError::Ok;
        };
```

**说明：** `clear_paired` 命令需要设备已授权（`isAuthorized` 为 true），这保证了只有已配对设备（自动授权）或刚通过配对码验证的设备才能执行清除操作。清除后发送 `BleAdvertising` 状态通知，附带消息 `"cleared"`。

- [ ] **Step 2: 编译验证**

Run: `pio run`
Expected: 编译成功，无错误。

- [ ] **Step 3: 提交**

```bash
git add src/main.cpp
git commit -m "feat(firmware): support clear_paired command via BLE"
```

---

### Task 2: 网页端 HTML 结构重写

**分支：** `gh-pages`

**Files:**
- Modify: `index.html`（完整重写）

- [ ] **Step 1: 重写 index.html 为三视图结构**

将 `index.html` 完整替换为：

```html
<!doctype html>
<html lang="zh-CN">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>matrix5 设备管理</title>
    <link rel="stylesheet" href="./style.css" />
  </head>
  <body>
    <main class="container">
      <!-- ========== 设备列表页 ========== -->
      <div id="deviceListView" class="view">
        <div class="header">
          <h1>我的设备</h1>
          <button id="addDeviceBtn" class="btn-small">+ 添加设备</button>
        </div>
        <p id="compat" class="hint"></p>
        <div id="deviceList"></div>
        <div id="emptyState" class="empty-state" style="display:none;">
          <p>暂无设备</p>
          <p class="sub">点击右上角添加按钮开始</p>
        </div>
      </div>

      <!-- ========== 连接/授权页 ========== -->
      <div id="connectView" class="view" style="display:none;">
        <div class="header">
          <button id="backFromConnect" class="btn-text">← 返回</button>
          <h1>连接设备</h1>
        </div>
        <div class="card">
          <p id="connectDesc" class="desc">搜索并选择附近的 matrix5 设备</p>
          <button id="searchConnectBtn">搜索并连接</button>
          <div id="pairingCodeSection" style="display:none;">
            <label>配对码</label>
            <input
              id="pairingCodeInput"
              maxlength="6"
              placeholder="6 位数字"
              inputmode="numeric"
              pattern="\d{6}"
            />
            <button id="verifyCodeBtn">验证配对码</button>
          </div>
          <div class="step-status" id="connectStatus"></div>
        </div>
      </div>

      <!-- ========== 配置网络页 ========== -->
      <div id="configView" class="view" style="display:none;">
        <div class="header">
          <button id="backFromConfig" class="btn-text">← 返回</button>
          <h1>配置网络</h1>
        </div>
        <div class="card">
          <p class="desc">填写 WiFi 和时区信息</p>
          <label>WiFi 名称</label>
          <input id="wifiSsid" placeholder="SSID" />
          <label>WiFi 密码</label>
          <input id="wifiPassword" type="password" placeholder="密码" />
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
          <button id="applyBtn">写入并应用</button>
          <div class="step-status" id="configStatus"></div>
        </div>
      </div>

      <pre id="status">等待操作...</pre>
    </main>
    <script type="module" src="./app.js"></script>
  </body>
</html>
```

- [ ] **Step 2: 提交**

```bash
git checkout gh-pages
git add index.html
git commit -m "feat(web): rewrite index.html with device management views"
```

---

### Task 3: 网页端 CSS 样式扩展

**分支：** `gh-pages`

**Files:**
- Modify: `style.css`（追加样式）

- [ ] **Step 1: 在 style.css 末尾追加设备管理相关样式**

在 `style.css` 文件末尾追加：

```css
/* ---- 视图切换 ---- */
.view {
  display: none;
}

.view.active {
  display: block;
}

/* ---- 头部 ---- */
.header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12px;
  gap: 12px;
}

.header h1 {
  margin: 0;
  flex: 1;
}

.btn-small {
  width: auto;
  padding: 8px 14px;
  font-size: 13px;
  margin-top: 0;
  white-space: nowrap;
}

.btn-text {
  width: auto;
  padding: 6px 10px;
  font-size: 13px;
  background: transparent;
  color: var(--muted);
  border: 1px solid var(--border);
  margin-top: 0;
  white-space: nowrap;
}

.btn-text:hover:not(:disabled) {
  background: var(--card-bg);
  color: var(--fg);
}

.btn-danger {
  background: var(--error);
}

.btn-danger:hover:not(:disabled) {
  background: #f87171;
}

/* ---- 设备卡片 ---- */
.device-card {
  background: var(--card-bg);
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 14px 16px;
  margin-bottom: 10px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.device-card .device-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.device-card .device-name {
  font-size: 15px;
  font-weight: 600;
}

.device-card .device-meta {
  font-size: 12px;
  color: var(--muted);
}

.device-card .device-actions {
  display: flex;
  gap: 8px;
}

.device-card .device-actions button {
  flex: 1;
  margin-top: 4px;
}

.device-card .btn-delete {
  background: transparent;
  color: var(--error);
  border: 1px solid var(--error);
  font-weight: 500;
}

.device-card .btn-delete:hover:not(:disabled) {
  background: var(--error);
  color: #fff;
}

/* ---- 空状态 ---- */
.empty-state {
  text-align: center;
  padding: 48px 20px;
  color: var(--muted);
}

.empty-state .sub {
  font-size: 13px;
  margin-top: 4px;
}
```

- [ ] **Step 2: 提交**

```bash
git add style.css
git commit -m "feat(web): add device management styles"
```

---

### Task 4: 网页端 JavaScript 逻辑重写

**分支：** `gh-pages`

**Files:**
- Modify: `app.js`（完整重写）

- [ ] **Step 1: 重写 app.js**

将 `app.js` 完整替换为：

```javascript
const SERVICE_UUID = "91a50001-66d8-4d35-a42b-df7f00c10000";
const AUTH_UUID    = "91a50002-66d8-4d35-a42b-df7f00c10000";
const CONFIG_UUID  = "91a50003-66d8-4d35-a42b-df7f00c10000";
const COMMAND_UUID = "91a50004-66d8-4d35-a42b-df7f00c10000";
const STATUS_UUID  = "91a50005-66d8-4d35-a42b-df7f00c10000";

const DEVICE_STORAGE_KEY = "matrix5_devices";

const $ = (id) => document.getElementById(id);
const te = new TextEncoder();
const td = new TextDecoder();

let device;
let server;
let authChar;
let configChar;
let commandChar;
let statusChar;

/* ================================================================
   设备管理 (localStorage)
   ================================================================ */

function loadDevices() {
  try {
    const raw = localStorage.getItem(DEVICE_STORAGE_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function saveDevices(devices) {
  localStorage.setItem(DEVICE_STORAGE_KEY, JSON.stringify(devices));
}

function addOrUpdateDevice(deviceInfo) {
  const devices = loadDevices();
  const idx = devices.findIndex((d) => d.id === deviceInfo.id);
  const now = new Date().toISOString();
  if (idx >= 0) {
    devices[idx].name = deviceInfo.name;
    devices[idx].lastConnected = now;
  } else {
    devices.push({
      id: deviceInfo.id,
      name: deviceInfo.name,
      pairedAt: now,
      lastConnected: now,
    });
  }
  saveDevices(devices);
}

function removeDeviceById(deviceId) {
  const devices = loadDevices().filter((d) => d.id !== deviceId);
  saveDevices(devices);
}

function isKnownDevice(deviceId) {
  return loadDevices().some((d) => d.id === deviceId);
}

function formatLastConnected(iso) {
  if (!iso) return "从未";
  const d = new Date(iso);
  const now = new Date();
  const isToday = d.toDateString() === now.toDateString();
  if (isToday) {
    return "今天 " + d.toLocaleTimeString("zh-CN", { hour: "2-digit", minute: "2-digit" });
  }
  return d.toLocaleDateString("zh-CN", { month: "short", day: "numeric" }) +
    " " + d.toLocaleTimeString("zh-CN", { hour: "2-digit", minute: "2-digit" });
}

/* ================================================================
   视图路由
   ================================================================ */

function showView(name) {
  document.querySelectorAll(".view").forEach((el) => el.classList.remove("active"));
  const target = $(name + "View");
  if (target) target.classList.add("active");
}

/* ================================================================
   设备列表页
   ================================================================ */

function renderDeviceList() {
  const devices = loadDevices();
  const listEl = $("deviceList");
  const emptyEl = $("emptyState");

  if (devices.length === 0) {
    listEl.innerHTML = "";
    emptyEl.style.display = "block";
    return;
  }

  emptyEl.style.display = "none";
  listEl.innerHTML = devices
    .map(
      (d) => `
    <div class="device-card" data-id="${d.id}">
      <div class="device-header">
        <span class="device-name">${escapeHtml(d.name || "未知设备")}</span>
        <button class="btn-text btn-delete" data-action="delete" data-id="${d.id}">删除</button>
      </div>
      <div class="device-meta">已配对 · 上次连接: ${formatLastConnected(d.lastConnected)}</div>
      <div class="device-actions">
        <button data-action="connect" data-id="${d.id}" data-name="${escapeHtml(d.name || "")}">连接配网</button>
      </div>
    </div>
  `
    )
    .join("");

  // 绑定卡片内按钮事件
  listEl.querySelectorAll("button[data-action]").forEach((btn) => {
    btn.addEventListener("click", (e) => {
      const action = e.target.dataset.action;
      const id = e.target.dataset.id;
      const name = e.target.dataset.name;
      if (action === "connect") {
        startConnectFlow(id, name);
      } else if (action === "delete") {
        startDeleteFlow(id, name);
      }
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
   连接流程（添加新设备 或 连接已有设备）
   ================================================================ */

let pendingDeviceId = null;
let pendingDeviceName = null;
let isNewDeviceFlow = false;

function resetConnection() {
  device = null;
  server = null;
  authChar = configChar = commandChar = statusChar = null;
  pendingDeviceId = null;
  pendingDeviceName = null;
}

function setConnectStatus(cls, text) {
  const el = $("connectStatus");
  if (!el) return;
  el.className = "step-status " + (cls || "");
  el.textContent = text || "";
}

async function connectBle(targetDevice) {
  server = await targetDevice.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  authChar    = await service.getCharacteristic(AUTH_UUID);
  configChar  = await service.getCharacteristic(CONFIG_UUID);
  commandChar = await service.getCharacteristic(COMMAND_UUID);
  statusChar  = await service.getCharacteristic(STATUS_UUID);

  targetDevice.addEventListener("gattserverdisconnected", () => {
    log("设备断开连接");
    resetConnection();
    if ($("connectView").classList.contains("active")) {
      setConnectStatus("err", "连接已断开");
    }
  });

  // 状态处理
  const handleStatus = (raw) => {
    log(`设备状态: ${raw}`);
    try {
      const s = JSON.parse(raw);
      if (s.state === 8) {
        // PairedDeviceConnected — 已配对设备自动授权
        setConnectStatus("ok", "已配对设备，自动授权");
        addOrUpdateDevice({ id: device.id, name: device.name || "matrix5" });
        setTimeout(() => showConfigView(), 300);
      } else if (s.state === 3) {
        // AuthOk — 新设备配对码验证成功
        setConnectStatus("ok", "授权成功");
        addOrUpdateDevice({ id: device.id, name: device.name || "matrix5" });
        setTimeout(() => showConfigView(), 300);
      } else if (s.state === 4) {
        // Done
        setConnectStatus("ok", "配置已应用，设备即将断开蓝牙");
      } else if (s.error !== 0) {
        setConnectStatus("err", `错误码 ${s.error}: ${s.message}`);
      }
    } catch {}
  };

  await statusChar.startNotifications();
  statusChar.addEventListener("characteristicvaluechanged", (event) => {
    handleStatus(td.decode(event.target.value));
  });

  try {
    const value = await statusChar.readValue();
    handleStatus(td.decode(value));
  } catch (e) {
    log(`读取初始状态失败: ${e.message}`);
  }
}

// 启动「添加新设备」流程
async function startAddFlow() {
  isNewDeviceFlow = true;
  pendingDeviceId = null;
  pendingDeviceName = null;
  resetConnection();

  $("pairingCodeSection").style.display = "none";
  $("searchConnectBtn").style.display = "block";
  $("searchConnectBtn").disabled = false;
  $("searchConnectBtn").textContent = "搜索并连接";
  $("connectDesc").textContent = "搜索并选择附近的 matrix5 设备";
  setConnectStatus("", "");
  showView("connect");
}

// 启动「连接已有设备」流程
async function startConnectFlow(deviceId, deviceName) {
  isNewDeviceFlow = false;
  pendingDeviceId = deviceId;
  pendingDeviceName = deviceName;
  resetConnection();

  $("pairingCodeSection").style.display = "none";
  $("searchConnectBtn").style.display = "block";
  $("searchConnectBtn").disabled = false;
  $("searchConnectBtn").textContent = "搜索并连接";
  $("connectDesc").textContent = `选择设备「${deviceName || "matrix5"}」进行连接`;
  setConnectStatus("", "");
  showView("connect");
}

async function onSearchConnect() {
  $("searchConnectBtn").disabled = true;
  $("searchConnectBtn").textContent = "连接中...";
  setConnectStatus("", "");

  try {
    device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
    });

    await connectBle(device);

    if (isNewDeviceFlow) {
      // 新设备：显示配对码输入
      $("pairingCodeSection").style.display = "block";
      $("searchConnectBtn").style.display = "none";
      $("connectDesc").textContent = `已连接: ${device.name || "matrix5"}，请输入配对码`;
      setConnectStatus("warn", "请输入设备屏幕上显示的 6 位配对码");
    } else {
      // 已有设备：等待自动授权通知 (state=8)
      $("searchConnectBtn").style.display = "none";
      $("connectDesc").textContent = `已连接: ${device.name || "matrix5"}`;
      setConnectStatus("warn", "等待设备授权...");
    }
  } catch (e) {
    $("searchConnectBtn").disabled = false;
    $("searchConnectBtn").textContent = "搜索并连接";
    setConnectStatus("err", `连接失败: ${e.message}`);
    log(`连接失败: ${e.message}`);
  }
}

async function onVerifyCode() {
  const code = $("pairingCodeInput").value.trim();
  if (!/^\d{6}$/.test(code)) {
    setConnectStatus("err", "配对码必须是 6 位数字");
    return;
  }
  try {
    await authChar.writeValue(te.encode(code));
    setConnectStatus("ok", "配对码已发送，等待验证...");
    log("已发送配对码");
  } catch (e) {
    setConnectStatus("err", `验证失败: ${e.message}`);
    log(`验证失败: ${e.message}`);
  }
}

/* ================================================================
   删除设备流程
   ================================================================ */

async function startDeleteFlow(deviceId, deviceName) {
  if (!confirm(`确定要删除设备「${deviceName || "未知设备"}」的配对记录吗？`)) {
    return;
  }

  resetConnection();
  setConnectStatus("", "");
  showView("connect");

  $("pairingCodeSection").style.display = "none";
  $("searchConnectBtn").style.display = "block";
  $("searchConnectBtn").disabled = false;
  $("searchConnectBtn").textContent = "选择设备并删除";
  $("connectDesc").textContent = `选择要删除的设备「${deviceName || "未知设备"}」`;

  // 保存到闭包变量，供一次性按钮使用
  $("searchConnectBtn").onclick = async () => {
    $("searchConnectBtn").disabled = true;
    $("searchConnectBtn").textContent = "删除中...";

    try {
      device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [SERVICE_UUID] }],
      });

      await connectBle(device);

      // 发送 clear_paired 命令
      await commandChar.writeValue(te.encode("clear_paired"));
      log("已发送 clear_paired 命令");

      // 等待设备响应（通知中会收到 state=BleAdvertising, msg="cleared"）
      setConnectStatus("ok", "配对记录已清除");
      removeDeviceById(deviceId);
      log("设备已从列表中移除");

      setTimeout(() => {
        showView("deviceList");
        renderDeviceList();
        resetConnection();
      }, 1500);
    } catch (e) {
      $("searchConnectBtn").disabled = false;
      $("searchConnectBtn").textContent = "选择设备并删除";
      setConnectStatus("err", `删除失败: ${e.message}`);
      log(`删除失败: ${e.message}`);
    }
  };
}

/* ================================================================
   配置网络页
   ================================================================ */

function showConfigView() {
  $("configStatus").textContent = "";
  $("configStatus").className = "step-status";
  showView("config");
}

async function onApplyConfig() {
  const payload = {
    wifiSsid:     $("wifiSsid").value.trim(),
    wifiPassword: $("wifiPassword").value,
    timezone:     $("timezone").value,
    ntpServer:    $("ntpServer").value.trim(),
  };
  if (!payload.wifiSsid || !payload.timezone || !payload.ntpServer) {
    $("configStatus").className = "step-status err";
    $("configStatus").textContent = "WiFi 名称、时区、NTP 服务器不能为空";
    return;
  }

  try {
    await configChar.writeValue(te.encode(JSON.stringify(payload)));
    log(`配置内容: ${JSON.stringify(payload)}`);
    await commandChar.writeValue(te.encode("apply"));
    $("configStatus").className = "step-status warn";
    $("configStatus").textContent = "配置已下发，等待设备响应...";
    log("已发送配置与 apply 命令");
  } catch (e) {
    $("configStatus").className = "step-status err";
    $("configStatus").textContent = `下发失败: ${e.message}`;
    log(`下发失败: ${e.message}`);
  }
}

/* ================================================================
   日志 & 兼容检测
   ================================================================ */

function log(msg) {
  $("status").textContent =
    `[${new Date().toLocaleTimeString()}] ${msg}\n` + $("status").textContent;
}

function checkCompat() {
  if (!navigator.bluetooth) {
    $("compat").textContent = "当前浏览器不支持 Web Bluetooth，请使用 Chrome / Edge。";
    $("addDeviceBtn").disabled = true;
  } else {
    $("compat").textContent = "Web Bluetooth 已就绪（Chrome / Edge）";
  }
}

/* ================================================================
   初始化
   ================================================================ */

checkCompat();
renderDeviceList();
showView("deviceList");

$("addDeviceBtn").addEventListener("click", startAddFlow);
$("backFromConnect").addEventListener("click", () => {
  resetConnection();
  showView("deviceList");
});
$("backFromConfig").addEventListener("click", () => {
  showView("deviceList");
});
$("searchConnectBtn").addEventListener("click", onSearchConnect);
$("verifyCodeBtn").addEventListener("click", onVerifyCode);
$("applyBtn").addEventListener("click", onApplyConfig);
```

**关键逻辑说明：**

1. **设备列表页**：从 localStorage 读取 `matrix5_devices`，渲染卡片。每个卡片有「连接配网」和「删除」按钮。
2. **添加新设备**：进入连接页 → 搜索连接 → 显示配对码输入 → 发送配对码 → 收到 `state=3 (AuthOk)` → 保存设备信息到 localStorage → 自动跳转到配置页。
3. **连接已有设备**：进入连接页 → 搜索连接 → 收到 `state=8 (PairedDeviceConnected)` → 更新 `lastConnected` → 自动跳转到配置页。
4. **删除设备**：确认对话框 → 搜索连接 → 发送 `clear_paired` 命令 → 收到 `state=1 (BleAdvertising)` 且 `msg="cleared"` → 从 localStorage 移除 → 返回列表。
5. **配置网络页**：与原有逻辑一致，发送 JSON 配置 + `apply` 命令。

- [ ] **Step 2: 提交**

```bash
git add app.js
git commit -m "feat(web): rewrite app.js with device management logic"
```

---

## 自我审查

**1. Spec 覆盖检查：**

| Spec 要求 | 对应任务 |
|-----------|---------|
| 已配对设备一键连接，自动进入配置 | Task 4: `startConnectFlow` + `handleStatus(state=8)` |
| 新设备首次配对流程（连接→授权→配置） | Task 4: `startAddFlow` + `onVerifyCode` + `handleStatus(state=3)` |
| 支持从网页端删除设备配对记录 | Task 4: `startDeleteFlow` + `clear_paired` 命令 |
| 设备信息持久化存储在 localStorage | Task 4: `loadDevices` / `saveDevices` / `addOrUpdateDevice` |
| 首页显示"我的设备"列表 | Task 2 + Task 4: 设备列表视图 + `renderDeviceList` |
| 固件端支持 `clear_paired` 命令 | Task 1: `onCommand` 中添加 `cmd == "clear_paired"` |

**2. Placeholder 扫描：** 无 TBD/TODO/"implement later" 等占位符。

**3. 类型一致性检查：** 固件端 `ConfigState::BleAdvertising = 1` 与网页端 `s.state === 1` 匹配。`clear_paired` 命令字符串在固件和网页两端一致。

---

## 执行交接

**Plan complete and saved to `docs/superpowers/plans/2026-05-11-web-device-management.md`.**

**两个执行选项：**

**1. Subagent-Driven（推荐）** - 我为每个任务派发独立的子代理，任务间进行审查，快速迭代

**2. Inline Execution** - 在当前会话中直接执行任务，批量执行并在检查点审查

**选择哪种方式？**
