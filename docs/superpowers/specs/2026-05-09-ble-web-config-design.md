# M5StickS3 BLE 网页配网设计文档

## 1. 背景与目标

matrix5 当前存在基于 WiFi AP 的配网思路。现计划彻底替换为 BLE + 网页配网：

- 设备端：M5StickS3 提供 BLE GATT 配网服务
- 网页端：部署在 GitHub Pages，通过 Chrome Web Bluetooth 与设备直连
- 配置项：WiFi SSID、WiFi 密码、时区、NTP 服务器
- 浏览器范围：仅支持 Chrome

本设计目标是以最小复杂度落地首版（方案 A）：自定义 GATT + JSON 配置写入 + 会话状态机。

## 2. 范围与非目标

### 2.1 范围

- 开机在无有效网络配置时自动进入 BLE 配网模式
- 设备支持按键长按手动重入 BLE 配网模式
- 使用一次性 6 位配网码进行授权
- 通过网页下发并应用 4 个配置项
- 完全移除旧 WiFi 配网入口

### 2.2 非目标

- 不引入云端后端或账号体系
- 不支持 Safari/Firefox/移动浏览器
- 首版不做二进制协议、分包协议、多语言 UI
- 首版不扩展设备偏好项（亮度、语言等）

## 3. 总体架构

### 3.1 设备端模块

- `ble_config_service.*`
  - 建立 GATT Service 与 Characteristic
  - 管理连接会话、授权状态、超时与断连
- `pairing_code.*`
  - 生成 6 位一次性配网码
  - 验证有效期（默认 120 秒）并在成功后立即失效
- `config_store.*`
  - 使用 NVS/Preferences 存储配置
  - 提供配置有效性与版本管理
- `net_apply.*`
  - 按配置连接 WiFi 并执行 NTP 同步
  - 返回统一错误码

`src/main.cpp` 仅保留流程编排：启动判断、进入配网、状态显示、主循环调度。

### 3.2 网页端模块

目录建议：`docs/web-config/`

- `index.html`：连接区、授权区、配置表单、状态输出区
- `app.js`：BLE 连接、授权、写配置、状态监听、错误映射
- `style.css`：基础布局与状态样式

发布方式：GitHub Pages 静态部署，无服务端。

### 3.3 边界与约束

- BLE 负责设备近场配置；不经过网络中转
- 网页端不持久化 WiFi 密码到远端
- 授权成功后才允许写入配置

## 4. BLE GATT 协议设计

## 4.1 Service 与 Characteristic

定义 1 个自定义 128-bit Service（UUID 在实现阶段统一常量定义），包含 4 个 Characteristic：

- `auth`（Write + Notify）
  - 写入：一次性 6 位配网码
  - 通知：授权结果
- `config`（Write）
  - 写入：UTF-8 JSON 配置
- `command`（Write）
  - 写入：控制命令（首版至少支持 `apply`）
- `status`（Read + Notify）
  - 输出：流程状态与错误码

## 4.2 配置 JSON

```json
{
  "wifiSsid": "YourWiFi",
  "wifiPassword": "YourPassword",
  "timezone": "Asia/Shanghai",
  "ntpServer": "ntp.aliyun.com"
}
```

字段约束：

- `wifiSsid`：1-32 字符
- `wifiPassword`：0-64 字符
- `timezone`：IANA 时区字符串
- `ntpServer`：域名或 IP

首版限制：单次 JSON 长度 <= 256 字节；超限直接拒绝。

## 4.3 错误码规范

- `0000`：成功
- `1001`：未授权
- `1002`：配网码错误
- `1003`：配网码过期
- `2001`：JSON 解析失败
- `2002`：字段缺失或非法
- `3001`：WiFi 连接失败
- `3002`：NTP 同步失败

## 4.4 状态机

`IDLE` -> `BLE_ADVERTISING` -> `AUTH_PENDING` -> `AUTH_OK` -> `CONFIG_RECEIVED` -> `APPLYING` -> `DONE`/`ERROR`

规则：

- 在配网窗口内，断连或超时可回到 `BLE_ADVERTISING`
- 成功后关闭配网会话、失效配网码

## 5. 设备端流程设计

## 5.1 启动流程

1. 读取本地配置
2. 若配置无效：自动进入 BLE 配网模式
3. 若配置有效：尝试连接 WiFi + NTP
4. 连接失败达到阈值后可回退 BLE 配网模式

## 5.2 手动重入流程

1. 监听长按按键事件
2. 断开当前网络连接（如有）
3. 进入 BLE 广播并显示配网码与剩余时间

## 5.3 配置应用流程

1. 网页完成授权后写 `config`
2. 设备校验并暂存配置
3. 收到 `command=apply`
4. 持久化配置 -> 连接 WiFi -> NTP 同步
5. 通过 `status` 回传成功/失败

## 6. 网页端交互流程

1. 用户点击连接设备（Web Bluetooth 设备选择器）
2. 建立 GATT 连接并订阅 `status`
3. 输入设备屏幕上的 6 位配网码并写入 `auth`
4. 授权通过后填写 4 个配置项并提交
5. 写入 `config` 与 `command=apply`
6. 页面显示实时状态与错误提示

兼容性处理：

- 若 `navigator.bluetooth` 不可用，直接提示“仅支持 Chrome”
- 若断连，提示用户重新连接并重试

## 7. 安全与可靠性

## 7.1 安全策略

- 使用一次性 6 位码，默认有效期 120 秒
- 未授权状态拒绝所有配置写入
- 授权成功后配网码立即失效
- 不在网页或设备日志打印明文 WiFi 密码

## 7.2 可靠性策略

- 设备端统一错误码，网页端统一映射人类可读文案
- 配置校验失败时不写入持久化存储
- WiFi 失败与 NTP 失败分开上报，便于排查

## 8. 旧逻辑替换策略

按需求彻底替换：

- 移除原 AP/HTTP 配网入口与相关调用路径
- 删除仅服务于旧配网方案的状态分支
- 保留与时钟显示相关的联网能力（WiFi/NTP）

## 9. 测试与验收

## 9.1 验收标准

- 首次开机可自动进入 BLE 配网
- 长按可重入 BLE 配网
- 一次性配网码鉴权生效（错码/过期可被拦截）
- 4 项配置可成功下发并生效
- 旧 WiFi 配网入口不可达
- 时钟主功能不回归

## 9.2 Demo 与测试文档

- `docs/demo-ble-config.md`：现场演示步骤与预期
- `test/test_ble_config_flow.md`：手工测试矩阵（正常流 + 异常流）

## 10. 风险与应对

- Web Bluetooth 平台差异
  - 仅声明支持 Chrome，文档写明版本建议
- BLE 连接不稳定
  - 网页端增加重连提示，设备端维持会话超时恢复
- 近场安全风险
  - 一次性码 + 时效控制 + 成功后立即失效

## 11. 里程碑建议

1. 完成设备端 GATT 框架与授权链路
2. 完成网页端连接/授权/配置提交流程
3. 完成配置应用与错误码闭环
4. 移除旧配网逻辑并完成回归测试
