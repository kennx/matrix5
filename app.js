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
   Device Management (localStorage)
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
   View Routing
   ================================================================ */

function showView(name) {
  document.querySelectorAll(".view").forEach((el) => el.classList.remove("active"));
  const target = $(name + "View");
  if (target) target.classList.add("active");
}

/* ================================================================
   Device List Page
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

  // Bind card button events
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
   Connection Flow (add new device or connect existing device)
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

  // Status handler
  const handleStatus = (raw) => {
    log(`设备状态: ${raw}`);
    try {
      const s = JSON.parse(raw);
      if (s.state === 8) {
        // PairedDeviceConnected — auto-auth for paired devices
        setConnectStatus("ok", "已配对设备，自动授权");
        addOrUpdateDevice({ id: device.id, name: device.name || "matrix5" });
        setTimeout(() => showConfigView(), 300);
      } else if (s.state === 3) {
        // AuthOk — new device pairing code verified
        setConnectStatus("ok", "授权成功");
        addOrUpdateDevice({ id: device.id, name: device.name || "matrix5" });
        setTimeout(() => showConfigView(), 300);
      } else if (s.state === 6) {
        // Done — 配置成功应用
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

// Start "Add New Device" flow
async function startAddFlow() {
  isNewDeviceFlow = true;
  pendingDeviceId = null;
  pendingDeviceName = null;
  resetConnection();

  $("pairingCodeSection").style.display = "none";
  $("searchConnectBtn").style.display = "block";
  $("searchConnectBtn").disabled = false;
  $("searchConnectBtn").textContent = "搜索并连接";
  $("searchConnectBtn").onclick = null;
  $("connectDesc").textContent = "搜索并选择附近的 matrix5 设备";
  setConnectStatus("", "");
  showView("connect");
}

// Start "Connect Existing Device" flow
async function startConnectFlow(deviceId, deviceName) {
  isNewDeviceFlow = false;
  pendingDeviceId = deviceId;
  pendingDeviceName = deviceName;
  resetConnection();

  $("pairingCodeSection").style.display = "none";
  $("searchConnectBtn").style.display = "block";
  $("searchConnectBtn").disabled = false;
  $("searchConnectBtn").textContent = "搜索并连接";
  $("searchConnectBtn").onclick = null;
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
      // New device: show pairing code input
      $("pairingCodeSection").style.display = "block";
      $("searchConnectBtn").style.display = "none";
      $("connectDesc").textContent = `已连接: ${device.name || "matrix5"}，请输入配对码`;
      setConnectStatus("warn", "请输入设备屏幕上显示的 6 位配对码");
    } else {
      // Existing device: wait for auto-auth notification (state=8)
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
   Delete Device Flow
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

  // Save to closure variable for one-time button use
  $("searchConnectBtn").onclick = async () => {
    $("searchConnectBtn").disabled = true;
    $("searchConnectBtn").textContent = "删除中...";

    try {
      device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [SERVICE_UUID] }],
      });

      await connectBle(device);

      // Send clear_paired command
      await commandChar.writeValue(te.encode("clear_paired"));
      log("已发送 clear_paired 命令");

      // Wait for device response (notification: state=BleAdvertising, msg="cleared")
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
   Config Network Page
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
   Log & Compatibility Check
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
   Initialization
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
