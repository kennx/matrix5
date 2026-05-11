# Web 端设备管理功能设计文档

## 概述

在网页端添加设备管理功能，支持添加、连接、删除已配对的 matrix5 设备。首页从三步向导改为"我的设备"列表，简化已配对设备的重复配网流程。

## 目标

- 已配对设备一键连接，自动进入配置网络步骤
- 新设备首次配对流程保持不变（连接 → 授权 → 配置）
- 支持从网页端删除设备配对记录
- 设备信息持久化存储在浏览器 localStorage

## 页面结构

### 首页 - 我的设备列表

显示所有已保存的设备卡片，每个卡片包含：
- 设备名称
- 配对状态（已配对 / 未配对）
- 上次连接时间
- "连接配网"按钮
- "删除"按钮

右上角"添加设备"按钮用于添加新设备。

### 添加/连接流程页

根据设备状态分两种流程：

**新设备（未配对）：**
1. 显示配对码输入框
2. 用户输入 6 位配对码
3. 验证成功后保存到 localStorage
4. 自动进入配置网络步骤

**已配对设备：**
1. 自动授权
2. 直接进入配置网络步骤

### 配置网络页

与当前步骤 3 保持一致：
- WiFi 名称 / 密码
- 时区选择
- NTP 服务器
- 写入并应用按钮

## 交互流程

| 操作 | 流程 |
|------|------|
| **添加新设备** | 点击"添加" → `requestDevice()` 选择设备 → 连接 → 显示配对码输入 → 验证 → 保存到 localStorage → 进入配置 |
| **连接已有设备** | 点击"连接配网" → `requestDevice()` 选择设备 → 连接 → 自动授权（state=8）→ 进入配置 |
| **删除设备** | 点击"删除" → 确认对话框 → `requestDevice()` 选择设备 → 连接 → 发送 `clear_paired` 命令 → 设备端清除配对记录 → 从 localStorage 移除 → 返回列表 |

## 固件端修改

### 支持 `clear_paired` 命令

在 `main.cpp` 的 `onCommand` 回调中添加：

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

## 数据模型

### localStorage 存储结构

```json
{
  "matrix5_devices": [
    {
      "id": "web-bluetooth-internal-device-id",
      "name": "matrix5-config",
      "pairedAt": "2026-05-11T10:30:00.000Z",
      "lastConnected": "2026-05-11T14:20:00.000Z"
    }
  ]
}
```

**字段说明：**
- `id`: Web Bluetooth API 返回的 device.id（同一会话内稳定）
- `name`: 设备名称
- `pairedAt`: 首次配对成功时间（ISO 8601）
- `lastConnected`: 上次连接时间（ISO 8601）

## 网页端文件变更

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `index.html` | 重写 | 从三步向导改为设备列表 + 流程页 |
| `app.js` | 重写 | 设备管理逻辑、页面路由、localStorage 操作 |
| `style.css` | 修改 | 添加设备列表卡片样式 |

## UI 设计

### 设备列表卡片

```
+----------------------------------+
| matrix5-config          [删除 ×] |
| 已配对 · 上次连接: 今天 10:30    |
|                                  |
| [      连接配网      ]           |
+----------------------------------+
```

### 空状态

```
+----------------------------------+
| 暂无设备                         |
| 点击右上角添加按钮开始           |
+----------------------------------+
```

## 错误处理

| 场景 | 处理方式 |
|------|---------|
| 删除时设备未连接 | 提示用户确保设备在附近并进入配网模式 |
| 删除命令失败 | 显示错误信息，不移除 localStorage 记录 |
| localStorage 损坏 | 清空列表，重新添加设备 |
| Web Bluetooth 不支持 | 显示浏览器兼容性提示 |

## 安全考虑

- `clear_paired` 命令需要设备已授权（已配对设备自动授权）
- 删除操作需要用户二次确认
- localStorage 中的设备 ID 仅用于 UI 展示，不影响实际连接

## 测试用例

| 用例 | 步骤 | 预期 |
|------|------|------|
| TC-01 | 添加新设备 | 设备出现在列表中，状态为已配对 |
| TC-02 | 点击已配对设备连接 | 直接进入配置网络步骤 |
| TC-03 | 删除设备 | 设备从列表消失，设备端配对记录清除 |
| TC-04 | 删除后重新添加 | 需要重新输入配对码 |
| TC-05 | 刷新页面 | 设备列表保持不变 |
