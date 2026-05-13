# IMU 自动旋转屏幕方向设计

## 概述

利用 M5StickS3 内置的 6 轴 IMU（3 轴加速度计 + 3 轴陀螺仪）检测设备重力方向，自动切换屏幕显示方向。支持 **Landscape（横屏）** 和 **Portrait（竖屏）** 两种方向，通过 500ms 防抖避免手持抖动导致的频繁跳变。

## 目标

- 设备横握时自动切换为 landscape（240×135），保持现有显示布局不变
- 设备竖握时自动切换为 portrait（135×240），时间垂直堆叠显示
- 方向切换响应迅速但不抖动（500ms 稳定时间）
- 所有现有功能（时钟、日期、电量、番茄钟）在两种方向下均可正常使用

## 方向定义

| 方向 | 显示旋转值 | 分辨率 | 触发条件 |
|------|-----------|--------|---------|
| Landscape | `M5.Display.setRotation(1)` | 240×135 | \|ax\| > \|ay\|（重力主要沿 X 轴） |
| Portrait | `M5.Display.setRotation(0)` | 135×240 | \|ay\| > \|ax\|（重力主要沿 Y 轴） |

> 注：M5StickS3 的 IMU 坐标系与屏幕坐标系需要对齐。`ax`/`ay` 的映射关系需在首次硬件测试时验证，如方向相反可交换判断条件。

## 架构

### 新增模块

- `src/orientation.h` — `OrientationManager` 类声明
- `src/orientation.cpp` — IMU 采样、方向判断、防抖实现

### 修改模块

- `src/main.cpp` — 启用 IMU、集成方向管理器、绘制函数传入方向参数

## OrientationManager 设计

### 接口

```cpp
enum class ScreenOrientation { Landscape, Portrait };

class OrientationManager {
public:
    void begin();
    void update();

    ScreenOrientation getOrientation() const;
    bool orientationJustChanged();

    static constexpr unsigned long DEBOUNCE_MS = 500;

private:
    ScreenOrientation current_ = ScreenOrientation::Landscape;
    ScreenOrientation pending_ = ScreenOrientation::Landscape;
    unsigned long pendingSince_ = 0;
    bool justChanged_ = false;
};
```

### 初始化（begin）

`begin()` 仅读取当前 IMU 数据初始化方向状态，不做额外配置（IMU 由 `M5.begin(cfg)` 在 `cfg.internal_imu = true` 时已初始化）。

### 方向判断（update）

每帧调用：

1. 读取加速度计：`M5.Imu.getAccel(&ax, &ay, &az)`
2. 比较 \|ax\| 和 \|ay\|，得到瞬时方向。当 \|ax\| == \|ay\|（设备接近 45° 对角线）时，保持当前方向不变
3. 如果瞬时方向与 `current_` 不同：
   - 若 `pending_` 不同，重置 `pendingSince_`
   - 更新 `pending_`
4. 如果 `pending_` 已持续 500ms 且与 `current_` 不同：
   - `current_ = pending_`
   - `justChanged_ = true`
5. 否则 `justChanged_ = false`

### 防抖行为示例

```
t=0ms:  读数 Landscape  → current=Landscape

t=100ms: 读数 Portrait   → pending=Portrait, pendingSince=100

t=300ms: 读数 Landscape  → pending=Landscape, pendingSince=300
        （未超过 500ms，方向不变）

t=400ms: 读数 Portrait   → pending=Portrait, pendingSince=400

t=900ms: 读数 Portrait   → pending 持续 500ms
        → current=Portrait, justChanged=true
```

## 屏幕旋转与 Sprite 重建

当 `orientationJustChanged()` 返回 true 时，`loop()` 执行：

```cpp
if (orientationMgr.orientationJustChanged()) {
    auto orient = orientationMgr.getOrientation();
    int rotation = (orient == ScreenOrientation::Landscape) ? 1 : 0;
    M5.Display.setRotation(rotation);

    sprite.deleteSprite();
    sprite.createSprite(M5.Display.width(), M5.Display.height());

    forceRedraw = true;
}
```

**关键点**：
- Sprite 必须重建，因为 `width()` / `height()` 需要对调
- `drawBackgroundDots()` 依赖 `sprite.width()` / `height()`，自动适配新方向
- `drawConfigScreen()` / `drawStatusScreen()` 直接操作 `M5.Display`，`setRotation()` 后居中计算自动正确

## 绘制函数适配

所有 `drawXxx()` 函数增加 `ScreenOrientation` 参数。Landscape 分支保持现有代码完全不变。

### drawClock(timeStr, orient)

**Landscape（现有逻辑）**：
`"HH:MM:SS"` 横向居中，保持不变。

**Portrait（新增）**：
5 行垂直堆叠，每行 2 个数字居中，冒号旋转 90° 作为水平分隔。

```
      HH
       · ·   ← 冒号旋转 90°（水平两点）
      MM
       · ·
      SS
```

布局计算：
- 2 个数字宽 10 列，居中 `startCol = (27 - 10) / 2 = 8`
- 数字行高 7，冒号旋转后高 1，行间距 2
- 总高度：`7 + 2 + 1 + 2 + 7 + 2 + 1 + 2 + 7 = 31`
- 顶部留白：`startRow = (48 - 31) / 2 = 8`

### drawColonRotated(startCol, startRow)

当前 `COLON[4] = {0x00, 0x01, 0x00, 0x01}` 表示 4 行中第 2、4 行有亮点。旋转 90° 后变成 4 列中第 2、4 列有亮点，即水平排列的两个点。

```cpp
static void drawColonRotated(int16_t startCol, int16_t startRow) {
    drawDot(startCol + 1, startRow, WHITE);
    drawDot(startCol + 3, startRow, WHITE);
}
```

### drawDate(dateStr, orient)

**Landscape（现有逻辑）**：保持不变。

**Portrait（新增）**：解析为星期、月、日三部分，三行垂直堆叠。

例如 `"MON 05 13"`：
```
      MON
      05
      13
```

每部分横向居中，三部分整体垂直居中。

### drawBattery(batteryStr, orient)

**Landscape**：居中横向显示，现有逻辑不变。

**Portrait**：同样居中横向显示。最大 `"100%"` 需要 4 个字符（约 23 列），在 27 列宽度内可以放下，无需分支。

## main.cpp 集成

### setup()

```cpp
void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;   // 新增：启用内置 IMU
    M5.begin(cfg);
    Serial.begin(115200);

    orientationMgr.begin();    // 新增

    M5.Display.setRotation(1);
    sprite.createSprite(M5.Display.width(), M5.Display.height());
    // ... 其余不变
}
```

### loop()

```cpp
void loop() {
    M5.update();
    orientationMgr.update();   // 新增

    // 方向变化时重建 sprite
    if (orientationMgr.orientationJustChanged()) {
        // ... 重建逻辑见上文
    }

    // 所有绘制调用传入当前方向
    auto orient = orientationMgr.getOrientation();

    if (pomodoro.isActive()) {
        // ... 番茄钟逻辑不变
        drawClock(buf, orient);          // 修改：传入 orient
        drawBattery(modeName, orient);   // 修改：传入 orient
    } else {
        // ... 时钟/日期/电量逻辑
        drawClock(timeStr, orient);      // 修改
        drawDate(dateStr, orient);       // 修改
        drawBattery(batteryStr, orient); // 修改
    }
}
```

## 测试策略

### 单元测试（native 环境）

新增 `test/native/test_orientation/test_main.cpp`，测试 `OrientationManager` 的纯逻辑部分：

1. **方向判断**：
   - 输入 `ax=1.0, ay=0.0` → 期望 Landscape
   - 输入 `ax=0.0, ay=1.0` → 期望 Portrait
   - 输入 `ax=0.7, ay=0.7`（45° 对角线）→ 需定义明确行为（取绝对值较大者，或保持当前方向）

2. **防抖**：
   - 快速抖动（300ms 内 Landscape→Portrait→Landscape）→ 方向不变
   - 稳定保持 500ms → 方向切换
   - 临界值（刚好 500ms）→ 方向切换

3. **`orientationJustChanged()`**：
   - 方向变化后的首次 `update()` 返回 true
   - 第二次 `update()` 返回 false（除非再次变化）

### 硬件验证

1. 设备横握、竖握各 5 次，确认方向正确切换
2. 手持设备正常晃动，确认无意外跳变
3. 确认番茄钟运行中旋转不中断计时
4. 确认配置模式 / 状态提示页面旋转后居中正确

## 不包含

- 四个方向（不实现 reverse-landscape / reverse-portrait）
- 旋转动画过渡（瞬间切换）
- 用户手动锁定方向
- 通过陀螺仪计算精确倾斜角度（仅使用加速度计判断大致方向）
