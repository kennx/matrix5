# IMU 自动旋转屏幕方向实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 利用 M5StickS3 内置 IMU 检测设备重力方向，自动切换 landscape/portrait 屏幕方向，并适配所有绘制函数。

**Architecture:** 新增 `OrientationManager` 纯逻辑类（接收加速度输入、方向判断、500ms 防抖），`main.cpp` 集成 IMU 读取和方向切换（重建 sprite），所有 `drawXxx()` 函数增加方向参数并在 portrait 分支实现垂直布局。

**Tech Stack:** PlatformIO (ESP32-S3 Arduino), M5Unified, M5GFX, native Unity testing

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `src/orientation.h` | 创建 | `OrientationManager` 类声明 |
| `src/orientation.cpp` | 创建 | `OrientationManager` 实现（方向判断 + 防抖） |
| `test/native/test_orientation/test_main.cpp` | 创建 | `OrientationManager` 单元测试 |
| `src/main.cpp` | 修改 | 启用 IMU、集成方向管理、重建 sprite、更新所有 draw 调用 |

---

### Task 1: OrientationManager 核心逻辑

**Files:**
- Create: `src/orientation.h`
- Create: `src/orientation.cpp`
- Test: `test/native/test_orientation/test_main.cpp`

- [ ] **Step 1: 编写 `src/orientation.h`**

```cpp
#pragma once

#include <cstdint>

enum class ScreenOrientation { Landscape, Portrait };

class OrientationManager {
public:
    void update(float ax, float ay, unsigned long nowMs);

    ScreenOrientation getOrientation() const { return current_; }
    bool orientationJustChanged() const { return justChanged_; }

    static constexpr unsigned long DEBOUNCE_MS = 500;

private:
    ScreenOrientation current_ = ScreenOrientation::Landscape;
    ScreenOrientation pending_ = ScreenOrientation::Landscape;
    unsigned long pendingSince_ = 0;
    bool justChanged_ = false;
};
```

- [ ] **Step 2: 编写 `src/orientation.cpp`**

```cpp
#include "orientation.h"

static ScreenOrientation orientationFromAccel(float ax, float ay) {
    if (ax < 0) ax = -ax;
    if (ay < 0) ay = -ay;
    if (ax == ay) return ScreenOrientation::Landscape;  // 45° 对角线默认 landscape
    return (ax > ay) ? ScreenOrientation::Landscape : ScreenOrientation::Portrait;
}

void OrientationManager::update(float ax, float ay, unsigned long nowMs) {
    ScreenOrientation instant = orientationFromAccel(ax, ay);

    if (instant != current_) {
        if (instant != pending_) {
            pending_ = instant;
            pendingSince_ = nowMs;
        } else if (nowMs - pendingSince_ >= DEBOUNCE_MS) {
            current_ = pending_;
            justChanged_ = true;
            return;
        }
    } else {
        pending_ = current_;
    }

    justChanged_ = false;
}
```

- [ ] **Step 3: 编写测试文件骨架**

创建目录 `test/native/test_orientation/`，写入：

```cpp
#include "orientation.h"
#include "../../../src/orientation.cpp"

#include <cassert>

static void test_initial_state() {
    OrientationManager mgr;
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
    assert(!mgr.orientationJustChanged());
}

static void test_landscape_from_accel() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
}

static void test_portrait_from_accel() {
    OrientationManager mgr;
    mgr.update(0.0f, 1.0f, 0);
    assert(mgr.getOrientation() == ScreenOrientation::Portrait);
}

static void test_debounce_no_change_on_quick_flip() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);     // Landscape
    mgr.update(0.0f, 1.0f, 100);   // Portrait at t=100
    mgr.update(1.0f, 0.0f, 200);   // Landscape at t=200 (< 500ms)
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
}

static void test_debounce_changes_after_500ms() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);     // Landscape
    mgr.update(0.0f, 1.0f, 0);     // Portrait at t=0
    mgr.update(0.0f, 1.0f, 500);   // Portrait at t=500
    assert(mgr.getOrientation() == ScreenOrientation::Portrait);
    assert(mgr.orientationJustChanged());
}

static void test_just_changed_clears_on_next_update() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);
    mgr.update(0.0f, 1.0f, 0);
    mgr.update(0.0f, 1.0f, 500);
    assert(mgr.orientationJustChanged());

    mgr.update(0.0f, 1.0f, 501);
    assert(!mgr.orientationJustChanged());
}

static void test_diagonal_defaults_to_landscape() {
    OrientationManager mgr;
    mgr.update(0.7f, 0.7f, 0);
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
}

int main() {
    test_initial_state();
    test_landscape_from_accel();
    test_portrait_from_accel();
    test_debounce_no_change_on_quick_flip();
    test_debounce_changes_after_500ms();
    test_just_changed_clears_on_next_update();
    test_diagonal_defaults_to_landscape();
    return 0;
}
```

- [ ] **Step 4: 运行 native 测试**

Run: `pio test -e native --filter test_orientation`

Expected: 所有测试通过

- [ ] **Step 5: Commit**

```bash
git add src/orientation.h src/orientation.cpp test/native/test_orientation/
git commit -m "$(cat <<'EOF'
feat(orientation): 新增 OrientationManager 与单元测试

- 根据加速度计 ax/ay 判断 Landscape/Portrait
- 500ms 防抖避免抖动
- 7 个 native 单元测试覆盖方向判断和防抖逻辑
EOF
)"
```

---

### Task 2: main.cpp 集成 OrientationManager（基础部分）

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 添加 include 和全局实例**

在 `src/main.cpp` 的 include 区域添加：

```cpp
#include "orientation.h"
```

在全局变量区域（`Pomodoro pomodoro;` 附近）添加：

```cpp
OrientationManager orientationMgr;
```

- [ ] **Step 2: 修改 setup() 启用 IMU**

在 `setup()` 中：

```cpp
void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;   // 新增：启用内置 IMU
    M5.begin(cfg);
    // ... 其余不变
}
```

- [ ] **Step 3: 修改 loop() 读取 IMU 并更新方向**

在 `loop()` 开头（`M5.update();` 之后）添加：

```cpp
    M5.update();

    // 新增：IMU 方向检测
    float ax, ay, az;
    if (M5.Imu.isEnabled()) {
        M5.Imu.getAccel(&ax, &ay, &az);
        orientationMgr.update(ax, ay, millis());
    }

    // 新增：方向变化时重建 sprite
    static ScreenOrientation lastOrient = ScreenOrientation::Landscape;
    ScreenOrientation currentOrient = orientationMgr.getOrientation();
    if (currentOrient != lastOrient) {
        lastOrient = currentOrient;
        int rotation = (currentOrient == ScreenOrientation::Landscape) ? 1 : 0;
        M5.Display.setRotation(rotation);
        sprite.deleteSprite();
        sprite.createSprite(M5.Display.width(), M5.Display.height());
    }
```

> 使用 `lastOrient` 静态变量而不是 `orientationJustChanged()`，因为 `orientationJustChanged()` 只在 `update()` 调用后的单帧返回 true，而 sprite 重建需要在方向确定变化后的同一帧执行。两种方式等价，但 `lastOrient` 更直观。

- [ ] **Step 4: 编译验证（m5stick-s3 环境）**

Run: `pio run -e m5stick-s3`

Expected: 编译通过，无错误

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
feat(main): 集成 OrientationManager，启用 IMU

- setup() 中启用 cfg.internal_imu
- loop() 每帧读取加速度计并更新方向
- 方向变化时自动重建 sprite 并切换屏幕旋转
EOF
)"
```

---

### Task 3: drawClock 适配 portrait + drawColonRotated

**Files:**
- Modify: `src/main.cpp`（drawClock 函数和新增 drawColonRotated）

- [ ] **Step 1: 新增 `drawColonRotated` 辅助函数**

在 `drawColon` 函数下方添加：

```cpp
static void drawColonRotated(int16_t startCol, int16_t startRow) {
    drawDot(startCol + 1, startRow, WHITE);
    drawDot(startCol + 3, startRow, WHITE);
}
```

- [ ] **Step 2: 修改 `drawClock` 签名和 landscape 分支**

将现有 `drawClock`：

```cpp
static void drawClock(const char* timeStr) {
```

改为：

```cpp
static void drawClock(const char* timeStr, ScreenOrientation orient) {
    sprite.fillSprite(BLACK);
    drawBackgroundDots();

    if (orient == ScreenOrientation::Portrait) {
        // Portrait 布局在 Step 3 实现
    } else {
        // Landscape（现有逻辑，保持完全不变）
        int totalCols = 0;
        int len = 0;
        for (const char* p = timeStr; *p; p++) {
            totalCols += (*p == ':') ? 1 : 5;
            len++;
        }
        if (len > 1) totalCols += (len - 1);

        int startCol = (sprite.width() / DOT_SIZE - totalCols) / 2;
        if (startCol < 0) startCol = 0;
        int startRow = 10;
        int col = startCol;

        for (const char* p = timeStr; *p; p++) {
            if (*p == ':') {
                drawColon(col, startRow + 1);
                col += 1;
            } else {
                int idx = charIndex(*p);
                if (idx >= 0) {
                    drawChar(col, startRow, FONT[idx]);
                }
                col += 5;
            }
            if (*(p + 1) != '\0') {
                col += 1;
            }
        }
    }

    sprite.pushSprite(0, 0);
}
```

- [ ] **Step 3: 实现 portrait 分支**

在 portrait 分支中填入：

```cpp
    if (orient == ScreenOrientation::Portrait) {
        // 判断格式：时钟 "HH:MM:SS" 或番茄钟 "MM:SS"
        bool hasHours = (timeStr[2] == ':' && timeStr[5] == ':');

        int col = (sprite.width() / DOT_SIZE - 10) / 2;  // 2 个数字宽 10 列，居中
        int row = hasHours ? 8 : 12;  // 5 行或 3 行，调整顶部留白

        auto drawTwoDigits = [&](const char* p) {
            int idx0 = charIndex(p[0]);
            int idx1 = charIndex(p[1]);
            if (idx0 >= 0) drawChar(col, row, FONT[idx0]);
            if (idx1 >= 0) drawChar(col + 6, row, FONT[idx1]);
            row += 9;  // 7 行高 + 2 行间距
        };

        auto drawRotatedColon = [&]() {
            drawColonRotated(col + 3, row);
            row += 4;  // 1 行高 + 3 行间距
        };

        const char* p = timeStr;
        if (hasHours) {
            drawTwoDigits(p);     // HH
            drawRotatedColon();   // :
            p += 3;
        }
        drawTwoDigits(p);         // MM
        drawRotatedColon();       // :
        p += 3;
        drawTwoDigits(p);         // SS
    }
```

> lambda `drawTwoDigits` 和 `drawRotatedColon` 捕获 `col` 和 `row` 的引用。注意 `row` 是值捕获的 lambda 参数，需要用 mutable lambda 或改为函数对象。更简单的方式是直接内联代码，不用 lambda。

修正（不用 lambda，直接内联）：

```cpp
    if (orient == ScreenOrientation::Portrait) {
        bool hasHours = (timeStr[2] == ':' && timeStr[5] == ':');
        int col = (sprite.width() / DOT_SIZE - 10) / 2;
        int row = hasHours ? 8 : 12;

        const char* p = timeStr;

        // 辅助：绘制两个数字并下移 row
        auto drawPair = [&](const char* digits) {
            int idx0 = charIndex(digits[0]);
            int idx1 = charIndex(digits[1]);
            if (idx0 >= 0) drawChar(col, row, FONT[idx0]);
            if (idx1 >= 0) drawChar(col + 6, row, FONT[idx1]);
            row += 9;
        };

        if (hasHours) {
            drawPair(p);          // HH
            drawColonRotated(col + 3, row);
            row += 4;
            p += 3;
        }
        drawPair(p);              // MM
        drawColonRotated(col + 3, row);
        row += 4;
        p += 3;
        drawPair(p);              // SS
    }
```

> lambda `drawPair` 捕获 `col`（值）和 `row`（引用），这是合法的 C++。

- [ ] **Step 4: 更新所有 `drawClock` 调用点**

在 `main.cpp` 中搜索所有 `drawClock(` 调用，添加 `orientationMgr.getOrientation()` 参数：

1. `setup()` 中的 `drawClock("00:00:00");` → `drawClock("00:00:00", ScreenOrientation::Landscape);`
   > setup 时 orientationMgr 还未读取 IMU，默认传 Landscape 即可（初始状态）。

2. 番茄钟 Running 中的 `drawClock(buf);` → `drawClock(buf, orientationMgr.getOrientation());`

3. 番茄钟 Paused 中的 `drawClock(buf);` → `drawClock(buf, orientationMgr.getOrientation());`

4. 时钟模式中的 `drawClock(buf);` → `drawClock(buf, orientationMgr.getOrientation());`

- [ ] **Step 5: 编译验证（m5stick-s3 环境）**

Run: `pio run -e m5stick-s3`

Expected: 编译通过

- [ ] **Step 6: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
feat(draw): drawClock 支持 portrait 垂直堆叠，新增 drawColonRotated

- drawClock 增加 ScreenOrientation 参数
- landscape 分支保持现有逻辑完全不变
- portrait 分支：HH / : / MM / : / SS 垂直居中堆叠，冒号旋转 90°
- 番茄钟 MM:SS 同样适配（3 行堆叠）
- 更新所有 drawClock 调用点传入当前方向
EOF
)"
```

---

### Task 4: drawDate 适配 portrait

**Files:**
- Modify: `src/main.cpp`（drawDate 函数）

- [ ] **Step 1: 修改 `drawDate` 签名和 landscape 分支**

将现有 `drawDate`：

```cpp
static void drawDate(const char* dateStr) {
```

改为：

```cpp
static void drawDate(const char* dateStr, ScreenOrientation orient) {
    sprite.fillSprite(BLACK);
    drawBackgroundDots();

    if (orient == ScreenOrientation::Portrait) {
        // Portrait 布局在 Step 2 实现
    } else {
        // Landscape（现有逻辑，保持完全不变）
        int startCol = 2;
        int startRow = 10;
        int col = startCol;

        for (const char* p = dateStr; *p; p++) {
            if (*p == ' ') {
                col += 2;
            } else {
                int idx = charIndex(*p);
                if (idx >= 0) {
                    drawChar(col, startRow, FONT[idx]);
                }
                col += 5;
            }
            if (*(p + 1) != '\0' && *p != ' ') {
                col += 1;
            }
        }
    }

    sprite.pushSprite(0, 0);
}
```

- [ ] **Step 2: 实现 portrait 分支**

在 portrait 分支中填入：

```cpp
    if (orient == ScreenOrientation::Portrait) {
        // 解析 "MON 05 13" 为三部分
        char part1[8] = {0}, part2[8] = {0}, part3[8] = {0};
        sscanf(dateStr, "%s %s %s", part1, part2, part3);

        const char* parts[3] = { part1, part2, part3 };
        int widths[3] = { 0, 0, 0 };

        // 计算每部分宽度（字符数 * 5 + 间距）
        for (int i = 0; i < 3; i++) {
            int len = 0;
            for (const char* p = parts[i]; *p; p++) {
                if (charIndex(*p) >= 0) {
                    widths[i] += 5;
                    len++;
                }
            }
            if (len > 1) widths[i] += (len - 1);
        }

        int maxWidth = widths[0];
        if (widths[1] > maxWidth) maxWidth = widths[1];
        if (widths[2] > maxWidth) maxWidth = widths[2];

        int colBase = (sprite.width() / DOT_SIZE - maxWidth) / 2;
        int row = 10;  // 顶部留白
        int lineHeight = 9;  // 7 行高 + 2 行间距

        for (int i = 0; i < 3; i++) {
            int col = colBase + (maxWidth - widths[i]) / 2;  // 每行独立居中
            for (const char* p = parts[i]; *p; p++) {
                int idx = charIndex(*p);
                if (idx >= 0) {
                    drawChar(col, row, FONT[idx]);
                }
                col += 5;
                if (*(p + 1) != '\0') {
                    col += 1;
                }
            }
            row += lineHeight;
        }
    }
```

> 注意：`sscanf` 解析空格分隔的三个 token。如果 `getLocalTime` 失败回退到 `"--- -- --"`，同样能正确解析为 `"---"`、`"--"`、`"--"`。

- [ ] **Step 3: 更新 `drawDate` 调用点**

将 `main.cpp` 中的 `drawDate(buf);` 改为：

```cpp
drawDate(buf, orientationMgr.getOrientation());
```

- [ ] **Step 4: 编译验证（m5stick-s3 环境）**

Run: `pio run -e m5stick-s3`

Expected: 编译通过

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
feat(draw): drawDate 支持 portrait 三行垂直显示

- drawDate 增加 ScreenOrientation 参数
- landscape 分支保持现有逻辑完全不变
- portrait 分支：星期 / 月 / 日 三行独立居中
- 更新 drawDate 调用点传入当前方向
EOF
)"
```

---

### Task 5: drawBattery 签名更新 + 所有 draw 调用点最终同步

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 修改 `drawBattery` 签名**

将现有 `drawBattery`：

```cpp
static void drawBattery(const char* batteryStr) {
```

改为：

```cpp
static void drawBattery(const char* batteryStr, ScreenOrientation /*orient*/) {
```

> 添加未使用的 `orient` 参数保持接口一致性。portrait 下 drawBattery 的横向居中逻辑已自动适配（`sprite.width()` 变化后居中计算正确），无需分支。

函数体保持完全不变。

- [ ] **Step 2: 更新所有 `drawBattery` 调用点**

搜索 `main.cpp` 中所有 `drawBattery(` 调用：

1. `drawBattery(pomodoro.getModeDisplayName());` → `drawBattery(pomodoro.getModeDisplayName(), orientationMgr.getOrientation());`

2. `drawBattery(buf);`（电量显示）→ `drawBattery(buf, orientationMgr.getOrientation());`

- [ ] **Step 3: 编译验证（m5stick-s3 环境）**

Run: `pio run -e m5stick-s3`

Expected: 编译通过，无警告

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
feat(draw): drawBattery 增加方向参数，同步所有 draw 调用

- drawBattery 增加 ScreenOrientation 参数（portrait 无需分支，居中自动适配）
- 更新所有 drawBattery 调用点传入当前方向
- 所有绘制函数接口统一
EOF
)"
```

---

### Task 6: 最终验证与提交

**Files:**
- Modify: `src/main.cpp`（可能的小修复）

- [ ] **Step 1: 运行 native 测试确认未破坏现有测试**

Run: `pio test -e native`

Expected: 所有现有测试（config_store、timezone、config_json 等）和新增 test_orientation 均通过

- [ ] **Step 2: 运行 m5stick-s3 编译**

Run: `pio run -e m5stick-s3`

Expected: 编译通过

- [ ] **Step 3: 最终审查代码**

确认以下 checklist：
- [ ] `cfg.internal_imu = true` 已添加
- [ ] `orientationMgr.update(ax, ay, millis())` 在 `M5.update()` 之后调用
- [ ] 方向变化时 `sprite.deleteSprite()` / `createSprite()` 正确执行
- [ ] 所有 `drawClock` / `drawDate` / `drawBattery` 调用均传入了方向参数
- [ ] Landscape 分支代码完全未改动（复制粘贴自原代码）
- [ ] Portrait 分支布局计算正确（居中、间距）
- [ ] `drawColonRotated` 函数存在且被调用

- [ ] **Step 4: Commit（如有修改）**

```bash
git add -A
git commit -m "$(cat <<'EOF'
chore: IMU 自动旋转功能最终验证与调整

- 确认所有 native 测试通过
- 确认 m5stick-s3 编译通过
EOF
)"
```

---

## Self-Review

### 1. Spec coverage

| Spec 要求 | 对应任务 |
|-----------|---------|
| OrientationManager 组件（IMU 采样、方向判断、防抖） | Task 1 |
| 屏幕旋转与 Sprite 重建 | Task 2 |
| drawClock portrait：5 行垂直堆叠 | Task 3 |
| drawDate portrait：三行垂直 | Task 4 |
| drawBattery portrait：横向居中 | Task 5（无需分支） |
| drawColonRotated 辅助函数 | Task 3 |
| main.cpp 集成 | Task 2, 3, 4, 5 |
| 单元测试 | Task 1 |

无遗漏。

### 2. Placeholder scan

- 无 "TBD" / "TODO"
- 无 "implement later"
- 无 "add appropriate error handling"
- 无 "Write tests for the above"（所有测试代码均已提供）
- 无 "Similar to Task N"

### 3. Type consistency

- `ScreenOrientation` 在 `orientation.h` 中定义，所有 task 中使用一致
- `drawClock` / `drawDate` / `drawBattery` 签名统一为 `(const char*, ScreenOrientation)`
- `OrientationManager::update(float, float, unsigned long)` 签名一致

