const $ = (id) => document.getElementById(id);
const te = new TextEncoder();
const td = new TextDecoder();

let port = null;
let reader = null;
let writer = null;
let readLoopRunning = false;

const isConnected = () => Boolean(port && writer);

async function connectDevice() {
  try {
    port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: 0x303a }],
    });
    await port.open({ baudRate: 115200 });

    writer = port.writable.getWriter();

    $("connectBtn").style.display = "none";
    $("connectStatus").textContent = "已连接，等待设备响应...";
    $("connectStatus").className = "hint ok";

    startReadLoop();

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
        $("brightnessValue").textContent = $("brightness").value + "%";
        $("wifiSsid").value = msg.data.wifiSsid || "";
        $("wifiPassword").value = msg.data.wifiPassword || "";
        $("timezone").value = msg.data.timezone || "Asia/Shanghai";
        $("ntpServer").value = msg.data.ntpServer || "ntp.aliyun.com";
      }
      $("connectStatus").textContent = "配置已加载";
      $("configSection").style.display = "block";
    } else if (msg.type === "scan_result") {
      renderWifiScanList(Array.isArray(msg.networks) ? msg.networks : []);
    } else if (msg.type === "ok" || msg.type === "success") {
      $("applyStatus").className = "step-status ok";
      $("applyStatus").textContent = "配置已应用成功";
      $("applyBtn").disabled = false;
    } else if (msg.type === "error") {
      $("applyStatus").className = "step-status err";
      const errCode = msg.code ?? "unknown";
      const errMsg = msg.message || "未知错误";
      $("applyStatus").textContent = `错误码 ${errCode}: ${errMsg}`;
      $("applyBtn").disabled = false;
    }
  } catch (e) {
    console.error("Parse error:", line, e);
  }
}

async function sendJson(obj) {
  if (!writer) {
    throw new Error("设备未连接");
  }
  const data = JSON.stringify(obj) + "\n";
  await writer.write(te.encode(data));
}

function onDisconnected() {
  $("connectBtn").style.display = "block";
  $("connectStatus").textContent = "设备已断开";
  $("connectStatus").className = "hint err";
  $("configSection").style.display = "none";
  $("applyBtn").disabled = false;
  void cleanupConnection();
}

async function cleanupConnection() {
  const currentReader = reader;
  const currentWriter = writer;
  const currentPort = port;

  reader = null;
  writer = null;
  port = null;

  if (currentReader) {
    try {
      await currentReader.cancel();
    } catch (_e) {}
    try {
      currentReader.releaseLock();
    } catch (_e) {}
  }

  if (currentWriter) {
    try {
      currentWriter.releaseLock();
    } catch (_e) {}
  }

  if (currentPort && currentPort.readable) {
    try {
      await currentPort.close();
    } catch (_e) {}
  }
}

async function onScanWifi() {
  if (!isConnected()) {
    $("wifiScanStatus").style.display = "block";
    $("wifiScanStatus").textContent = "请先连接设备";
    return;
  }

  $("wifiScanStatus").style.display = "block";
  $("wifiScanStatus").textContent = "正在扫描...";
  $("wifiScanList").innerHTML = "";
  try {
    await sendJson({ cmd: "scan_wifi" });
  } catch (e) {
    $("wifiScanStatus").textContent = `扫描请求失败: ${e.message}`;
  }
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
  const safe = String(str ?? "");
  return safe
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

async function onApplyConfig() {
  if (!isConnected()) {
    $("applyStatus").className = "step-status err";
    $("applyStatus").textContent = "请先连接设备";
    return;
  }

  const payload = {
    wifiSsid: $("wifiSsid").value.trim(),
    wifiPassword: $("wifiPassword").value,
    timezone: $("timezone").value,
    ntpServer: $("ntpServer").value.trim(),
    brightness: parseInt($("brightness").value, 10),
  };

  if (!Number.isInteger(payload.brightness) || payload.brightness < 1 || payload.brightness > 100) {
    $("applyStatus").className = "step-status err";
    $("applyStatus").textContent = "亮度必须在 1 到 100 之间";
    return;
  }

  if (!payload.timezone) {
    $("applyStatus").className = "step-status err";
    $("applyStatus").textContent = "时区不能为空";
    return;
  }

  if (payload.wifiSsid && !payload.ntpServer) {
    $("applyStatus").className = "step-status err";
    $("applyStatus").textContent = "连接 WiFi 时 NTP 服务器不能为空";
    return;
  }

  $("applyStatus").className = "step-status warn";
  $("applyStatus").textContent = "正在应用配置...";
  $("applyBtn").disabled = true;

  try {
    await sendJson({
      cmd: "apply",
      config: payload,
      time: Math.floor(Date.now() / 1000),
    });
  } catch (e) {
    $("applyStatus").className = "step-status err";
    $("applyStatus").textContent = `下发失败: ${e.message}`;
    $("applyBtn").disabled = false;
  }
}

$("brightness").addEventListener("input", (e) => {
  $("brightnessValue").textContent = e.target.value + "%";
});

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
