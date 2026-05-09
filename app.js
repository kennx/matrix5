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
  if (!/^\d{6}$/.test(code)) {
    throw new Error("配网码必须是 6 位数字");
  }
  await authChar.writeValue(te.encode(code));
  log("已发送配网码");
}

async function applyConfig() {
  const payload = {
    wifiSsid: $("wifiSsid").value.trim(),
    wifiPassword: $("wifiPassword").value,
    timezone: $("timezone").value.trim(),
    ntpServer: $("ntpServer").value.trim(),
  };
  if (!payload.wifiSsid || !payload.timezone || !payload.ntpServer) {
    throw new Error("SSID / 时区 / NTP 不能为空");
  }
  await configChar.writeValue(te.encode(JSON.stringify(payload)));
  log(`配置内容: ${JSON.stringify(payload)}`);
  await commandChar.writeValue(te.encode("apply"));
  log("已发送配置与 apply 命令");
}

checkCompat();
$("connectBtn").addEventListener("click", () => connect().catch((e) => log(`连接失败: ${e.message}`)));
$("authBtn").addEventListener("click", () => auth().catch((e) => log(`授权失败: ${e.message}`)));
$("applyBtn").addEventListener("click", () => applyConfig().catch((e) => log(`下发失败: ${e.message}`)));
