const SERVICE_UUID = "91a50001-66d8-4d35-a42b-df7f00c10000";
const AUTH_UUID    = "91a50002-66d8-4d35-a42b-df7f00c10000";
const CONFIG_UUID  = "91a50003-66d8-4d35-a42b-df7f00c10000";
const COMMAND_UUID = "91a50004-66d8-4d35-a42b-df7f00c10000";
const STATUS_UUID  = "91a50005-66d8-4d35-a42b-df7f00c10000";

const $ = (id) => document.getElementById(id);
const te = new TextEncoder();
const td = new TextDecoder();

let device;
let server;
let authChar;
let configChar;
let commandChar;
let statusChar;

/* ---- 步骤状态管理 ---- */
function setStepStatus(stepNum, cls, text) {
  const el = $(`step${stepNum}Status`);
  if (!el) return;
  el.className = "step-status " + (cls || "");
  el.textContent = text || "";
}

function setStepActive(stepNum) {
  document.querySelectorAll(".step").forEach((el) => {
    const n = Number(el.dataset.step);
    el.classList.remove("active", "done");
    if (n < stepNum) el.classList.add("done");
    if (n === stepNum) el.classList.add("active");
  });

  for (let i = 1; i <= 3; i++) {
    const card = $(`step${i}`);
    const isDisabled = i > stepNum;
    card.classList.toggle("disabled", isDisabled);

    const inputs = card.querySelectorAll("input, select, button");
    inputs.forEach((inp) => {
      inp.disabled = isDisabled;
    });
  }
}

function resetToStep1(reason) {
  device = null;
  server = null;
  authChar = configChar = commandChar = statusChar = null;
  setStepActive(1);
  setStepStatus(1, "err", reason || "连接已断开");
  log("连接已断开，请重新连接设备");
}

/* ---- 日志 ---- */
function log(msg) {
  $("status").textContent = `[${new Date().toLocaleTimeString()}] ${msg}\n` + $("status").textContent;
}

/* ---- 浏览器兼容性 ---- */
function checkCompat() {
  if (!navigator.bluetooth) {
    $("compat").textContent = "当前浏览器不支持 Web Bluetooth，请使用 Chrome / Edge。";
    $("connectBtn").disabled = true;
  } else {
    $("compat").textContent = "Web Bluetooth 已就绪（Chrome / Edge）";
  }
}

/* ---- 连接 ---- */
async function connect() {
  device = await navigator.bluetooth.requestDevice({
    filters: [{ services: [SERVICE_UUID] }],
  });

  device.addEventListener("gattserverdisconnected", () => {
    resetToStep1("设备断开连接");
  });

  server = await device.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  authChar    = await service.getCharacteristic(AUTH_UUID);
  configChar  = await service.getCharacteristic(CONFIG_UUID);
  commandChar = await service.getCharacteristic(COMMAND_UUID);
  statusChar  = await service.getCharacteristic(STATUS_UUID);

  // 状态处理逻辑（通知 + 初始读取共用）
  const handleStatus = (raw) => {
    log(`设备状态: ${raw}`);
    try {
      const s = JSON.parse(raw);
      if (s.state === 8) {        // PairedDeviceConnected
        setStepStatus(2, "ok", "已配对设备，自动授权");
        setStepActive(3);
        log("检测到已配对设备，自动授权");
      } else if (s.state === 3) { // AuthOk
        setStepStatus(2, "ok", "授权成功");
        log("设备授权成功");
      } else if (s.state === 4) { // Done
        setStepStatus(3, "ok", "配置已应用，设备即将断开蓝牙");
        setStepActive(3);
      } else if (s.error !== 0) { // 任意错误
        setStepStatus(getCurrentStep(), "err", `错误码 ${s.error}: ${s.message}`);
      }
    } catch {}
  };

  await statusChar.startNotifications();
  statusChar.addEventListener("characteristicvaluechanged", (event) => {
    handleStatus(td.decode(event.target.value));
  });

  // 读取当前状态值（设备可能在订阅前已设置状态）
  try {
    const value = await statusChar.readValue();
    handleStatus(td.decode(value));
  } catch (e) {
    log(`读取初始状态失败: ${e.message}`);
  }

  setStepActive(2);
  setStepStatus(1, "ok", `已连接: ${device.name || "matrix5"}`);
  log("设备连接成功");
}

function getCurrentStep() {
  const active = document.querySelector(".step.active");
  return active ? Number(active.dataset.step) : 1;
}

/* ---- 授权 ---- */
async function auth() {
  const code = $("pairingCode").value.trim();
  if (!/^\d{6}$/.test(code)) {
    setStepStatus(2, "err", "配网码必须是 6 位数字");
    throw new Error("配网码必须是 6 位数字");
  }
  await authChar.writeValue(te.encode(code));
  setStepStatus(2, "ok", "授权成功");
  setStepActive(3);
  log("已发送配网码，授权成功");
}

/* ---- 配置 ---- */
async function applyConfig() {
  const payload = {
    wifiSsid:     $("wifiSsid").value.trim(),
    wifiPassword: $("wifiPassword").value,
    timezone:     $("timezone").value,
    ntpServer:    $("ntpServer").value.trim(),
  };
  if (!payload.wifiSsid || !payload.timezone || !payload.ntpServer) {
    setStepStatus(3, "err", "WiFi 名称、时区、NTP 服务器不能为空");
    throw new Error("SSID / 时区 / NTP 不能为空");
  }
  await configChar.writeValue(te.encode(JSON.stringify(payload)));
  log(`配置内容: ${JSON.stringify(payload)}`);
  await commandChar.writeValue(te.encode("apply"));
  setStepStatus(3, "warn", "配置已下发，等待设备响应...");
  log("已发送配置与 apply 命令");
}

/* ---- 事件绑定 ---- */
checkCompat();
setStepActive(1);

$("connectBtn").addEventListener("click", () =>
  connect().catch((e) => {
    setStepStatus(1, "err", `连接失败: ${e.message}`);
    log(`连接失败: ${e.message}`);
  })
);

$("authBtn").addEventListener("click", () =>
  auth().catch((e) => {
    setStepStatus(2, "err", `授权失败: ${e.message}`);
    log(`授权失败: ${e.message}`);
  })
);

$("applyBtn").addEventListener("click", () =>
  applyConfig().catch((e) => {
    setStepStatus(3, "err", `下发失败: ${e.message}`);
    log(`下发失败: ${e.message}`);
  })
);
