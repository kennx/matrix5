const $ = (id) => document.getElementById(id);
const te = new TextEncoder();
const td = new TextDecoder();

let port = null;
let reader = null;
let writer = null;
let readLoopPromise = null;
let readBuffer = "";

/* ================================================================
   Serial Connection
   ================================================================ */

function setConnectionStatus(cls, text) {
  const el = $("connectionStatus");
  if (!el) return;
  el.className = "step-status " + (cls || "");
  el.textContent = text || "";
}

function setApplyStatus(cls, text) {
  const el = $("applyStatus");
  if (!el) return;
  el.className = "step-status " + (cls || "");
  el.textContent = text || "";
}

async function connectDevice() {
  if (!navigator.serial) {
    setConnectionStatus("err", "当前浏览器不支持 Web Serial，请使用 Chrome / Edge 桌面版");
    return;
  }

  $("connectBtn").disabled = true;
  setConnectionStatus("", "正在请求 USB 设备...");

  try {
    port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: 0x303a }], // Espressif ESP32-S3
    });

    await port.open({ baudRate: 115200 });

    port.addEventListener("disconnect", () => {
      onDisconnected();
    });

    reader = port.readable.getReader();
    writer = port.writable.getWriter();

    readLoopPromise = readLoop();

    setConnectionStatus("ok", "设备已连接");
    $("configSection").style.display = "block";
    log("设备已连接");

    // Request current config from device
    await sendJson({ cmd: "get_config" });
  } catch (e) {
    $("connectBtn").disabled = false;
    setConnectionStatus("err", `连接失败: ${e.message}`);
    log(`连接失败: ${e.message}`);
  }
}

function onDisconnected() {
  log("设备已断开");
  setConnectionStatus("err", "设备已断开");
  $("configSection").style.display = "none";
  $("connectBtn").disabled = false;
  cleanupPort();
}

function cleanupPort() {
  if (reader) {
    reader.releaseLock();
    reader = null;
  }
  if (writer) {
    writer.releaseLock();
    writer = null;
  }
  port = null;
  readBuffer = "";
}

async function readLoop() {
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      readBuffer += td.decode(value, { stream: true });
      processBuffer();
    }
  } catch (e) {
    if (e.name !== "AbortError" && e.name !== "BreakError") {
      log(`读取错误: ${e.message}`);
    }
  }
}

function processBuffer() {
  let lines = readBuffer.split("\n");
  // Keep the last incomplete line in the buffer
  readBuffer = lines.pop();
  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      const msg = JSON.parse(trimmed);
      handleMessage(msg);
    } catch (e) {
      console.error("JSON parse error:", trimmed, e);
    }
  }
}

function handleMessage(msg) {
  log(`收到: ${JSON.stringify(msg)}`);

  if (msg.type === "config" && msg.data) {
    fillConfigForm(msg.data);
  } else if (msg.type === "scan_result" && Array.isArray(msg.networks)) {
    renderWifiScanList(msg.networks);
  } else if (msg.type === "error") {
    setApplyStatus("err", `错误 ${msg.code || ""}: ${msg.message || ""}`);
  } else if (msg.type === "ok" || msg.type === "success") {
    setApplyStatus("ok", msg.message || "操作成功");
  }
}

async function sendJson(obj) {
  if (!writer) return;
  const line = JSON.stringify(obj) + "\n";
  await writer.write(te.encode(line));
  log(`发送: ${line.trim()}`);
}

/* ================================================================
   Config Form
   ================================================================ */

function fillConfigForm(data) {
  if (data.brightness !== undefined) {
    $("brightness").value = data.brightness;
    $("brightnessValue").textContent = data.brightness + "%";
  }
  if (data.wifiSsid !== undefined) $("wifiSsid").value = data.wifiSsid;
  if (data.wifiPassword !== undefined) $("wifiPassword").value = data.wifiPassword;
  if (data.timezone !== undefined) $("timezone").value = data.timezone;
  if (data.ntpServer !== undefined) $("ntpServer").value = data.ntpServer;
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

async function onScanWifi() {
  $("wifiScanStatus").style.display = "block";
  $("wifiScanStatus").textContent = "正在扫描...";
  $("wifiScanList").innerHTML = "";
  await sendJson({ cmd: "scan_wifi" });
}

async function onApplyConfig() {
  const config = {
    brightness: parseInt($("brightness").value, 10),
    wifiSsid: $("wifiSsid").value.trim(),
    wifiPassword: $("wifiPassword").value,
    timezone: $("timezone").value,
    ntpServer: $("ntpServer").value.trim(),
  };

  if (!config.timezone || !config.ntpServer) {
    setApplyStatus("err", "时区和 NTP 服务器不能为空");
    return;
  }

  try {
    const payload = {
      cmd: "apply",
      config: config,
      time: Math.floor(Date.now() / 1000),
    };
    await sendJson(payload);
    setApplyStatus("warn", "配置已下发，等待设备响应...");
  } catch (e) {
    setApplyStatus("err", `下发失败: ${e.message}`);
    log(`下发失败: ${e.message}`);
  }
}

/* ================================================================
   Utilities
   ================================================================ */

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function log(msg) {
  $("status").textContent =
    `[${new Date().toLocaleTimeString()}] ${msg}\n` + $("status").textContent;
}

function checkCompat() {
  if (!navigator.serial) {
    $("compat").textContent = "当前浏览器不支持 Web Serial，请使用 Chrome / Edge 桌面版";
    $("connectBtn").disabled = true;
  } else {
    $("compat").textContent = "Web Serial 已就绪（Chrome / Edge 桌面版）";
  }
}

/* ================================================================
   Initialization
   ================================================================ */

checkCompat();

$("connectBtn").addEventListener("click", connectDevice);
$("scanWifiBtn").addEventListener("click", onScanWifi);
$("applyBtn").addEventListener("click", onApplyConfig);

$("brightness").addEventListener("input", (e) => {
  $("brightnessValue").textContent = e.target.value + "%";
});
