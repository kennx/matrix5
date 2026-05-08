# LED 点阵时钟实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 M5StickS3 的 5×5 圆形点阵背景上叠加显示 `00:00:00` 时钟，数字为 5×7 点阵、冒号为 1×4 点阵，使用 Sprite 双缓冲无闪烁渲染。

**Architecture:** 全量双缓冲方案。每帧在 M5Canvas Sprite 上先画 48×27 个暗色圆点作为背景，再遍历时间字符串查字体表，在对应网格位置画白色圆点覆盖，最后 `pushSprite` 整块推送到屏幕。保留原有 WiFi AP 配网和 NTP 时间同步逻辑。

**Tech Stack:** PlatformIO + Arduino (ESP32-S3) + M5Unified/M5GFX

---

## File Structure

| 文件 | 操作 | 职责 |
|------|------|------|
| `src/main.cpp` | 修改 | 字体数据、绘制函数、时钟逻辑、主程序流程 |

所有改动集中在 `main.cpp`。时钟功能与 WiFi 配置服务共用同一文件，通过函数拆分保持清晰边界。

---

### Task 1: 定义字体数据

**Files:**
- Modify: `src/main.cpp`

**Context:** 在 `main.cpp` 顶部、WiFi 配置页面上方，添加字体数据常量和字符索引函数。

- [ ] **Step 1: 添加 5×7 点阵字体数据（0-9 + A-Z）**

在 `// ========== LED 点阵背景参数 ==========` 上方插入：

```cpp
// ========== LED 点阵字体 ==========
// 5×7 点阵，每行低 5 位有效（bit4=左, bit0=右）
static const uint8_t FONT[36][7] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x0E, 0x11, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x04, 0x04, 0x04}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E}, // 9
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
};

static const uint8_t COLON[4] = {
    0x00, // 相对行 0: 空
    0x01, // 相对行 1: 点亮
    0x00, // 相对行 2: 空
    0x01, // 相对行 3: 点亮
};

static inline int charIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}
```

- [ ] **Step 2: 提交**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
feat: 添加 5x7 LED 点阵字体数据（0-9 + A-Z）和冒号点阵

- FONT[36][7]: 数字 0-9 + 字母 A-Z 的 5x7 点阵掩码
- COLON[4]: 1x4 冒号点阵，两个点亮位置
- charIndex(): 字符到字体表索引的映射函数
EOF
)"
```

---

### Task 2: 添加 Sprite 双缓冲和绘制函数

**Files:**
- Modify: `src/main.cpp`

**Context:** 在 `// ========== 绘制 LED 点阵背景 ==========` 上方添加 Sprite 声明和绘制函数。

- [ ] **Step 1: 声明全局 Sprite**

在 `bool apMode = false;` 下方插入：

```cpp
M5Canvas sprite(&M5.Display);
```

- [ ] **Step 2: 添加单点绘制辅助函数**

在 `drawDotMatrixBackground()` 函数上方插入：

```cpp
static void drawDot(int16_t col, int16_t row, uint16_t color) {
    int cx = col * DOT_SIZE + DOT_SIZE / 2;
    int cy = row * DOT_SIZE + DOT_SIZE / 2;
    sprite.fillCircle(cx, cy, DOT_RADIUS, color);
}
```

- [ ] **Step 3: 重写 drawDotMatrixBackground 为在 sprite 上绘制**

将现有 `drawDotMatrixBackground()` 替换为：

```cpp
static void drawBackgroundDots() {
    int w = sprite.width();
    int h = sprite.height();
    int cols = w / DOT_SIZE;
    int rows = h / DOT_SIZE;
    uint16_t dimColor = sprite.color565(DOT_BRIGHT, DOT_BRIGHT, DOT_BRIGHT);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            drawDot(col, row, dimColor);
        }
    }
}
```

- [ ] **Step 4: 添加字符绘制函数**

在 `drawBackgroundDots()` 下方插入：

```cpp
static void drawChar(int16_t startCol, int16_t startRow, const uint8_t mask[7]) {
    for (int row = 0; row < 7; row++) {
        uint8_t bits = mask[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col))) {
                drawDot(startCol + col, startRow + row, WHITE);
            }
        }
    }
}

static void drawColon(int16_t startCol, int16_t startRow) {
    for (int row = 0; row < 4; row++) {
        if (COLON[row]) {
            drawDot(startCol, startRow + row, WHITE);
        }
    }
}
```

- [ ] **Step 5: 添加 drawClock 函数**

在 `drawColon()` 下方插入：

```cpp
static void drawClock(const char* timeStr) {
    sprite.fillSprite(BLACK);
    drawBackgroundDots();

    int startCol = 3;   // (48 - 42) / 2 = 3
    int startRow = 10;  // (27 - 7) / 2 = 10
    int col = startCol;

    for (const char* p = timeStr; *p; p++) {
        if (*p == ':') {
            drawColon(col, startRow + 1);  // 冒号垂直居中偏移 1 行
            col += 1;
        } else {
            int idx = charIndex(*p);
            if (idx >= 0) {
                drawChar(col, startRow, FONT[idx]);
            }
            col += 5;
        }
    }

    sprite.pushSprite(0, 0);
}
```

- [ ] **Step 6: 提交**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
feat: 添加 Sprite 双缓冲和点阵字符绘制函数

- M5Canvas sprite 作为全屏双缓冲
- drawDot(): 在 sprite 上绘制单个大点
- drawBackgroundDots(): 绘制 48x27 暗色背景点
- drawChar(): 绘制 5x7 字符（数字/字母）
- drawColon(): 绘制 1x4 冒号
- drawClock(): 完整时钟渲染，居中布局
EOF
)"
```

---

### Task 3: 修改主程序集成时钟显示

**Files:**
- Modify: `src/main.cpp`

**Context:** 修改 `setup()` 初始化 Sprite 并显示默认时间，修改 `loop()` 每秒更新时间。

- [ ] **Step 1: 修改 setup()**

将 `setup()` 函数替换为：

```cpp
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    M5.Display.setRotation(1);

    sprite.setColorDepth(16);
    sprite.createSprite(M5.Display.width(), M5.Display.height());

    drawClock("00:00:00");

    prefs.begin("m5stick", false);

    String ssid = prefs.getString("ssid", "");
    Serial.printf("Saved SSID: %s\n", ssid.c_str());

    if (!tryConnectWiFi()) {
        Serial.println("WiFi failed, starting AP");
        startAPMode();
    } else {
        Serial.println("WiFi connected");
    }
}
```

- [ ] **Step 2: 修改 loop()**

将 `loop()` 函数替换为：

```cpp
void loop() {
    if (apMode) {
        server.handleClient();
    } else {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate >= 1000) {
            lastUpdate = millis();

            struct tm timeinfo;
            char buf[9];
            if (getLocalTime(&timeinfo)) {
                strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
            } else {
                strncpy(buf, "00:00:00", sizeof(buf));
            }

            drawClock(buf);
        }
    }
    M5.delay(10);
}
```

注意：`getLocalTime` 和 `strftime` 需要 `<time.h>` 头文件。检查 `main.cpp` 顶部是否已有 `#include <time.h>`，如果没有需要添加。

- [ ] **Step 3: 确认头文件完整**

确保 `main.cpp` 顶部包含以下头文件：

```cpp
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
```

如果缺少 `time.h`，添加它。

- [ ] **Step 4: 提交**

```bash
git add src/main.cpp
git commit -m "$(cat <<'EOF'
feat: 集成时钟显示到主程序

- setup(): 初始化 Sprite，显示默认时间 00:00:00
- loop(): 每秒获取 NTP 时间并更新显示
- 保留原有 WiFi AP 配网和 NTP 同步逻辑
EOF
)"
```

---

### Task 4: 编译验证

**Files:**
- 无文件修改

- [ ] **Step 1: 编译**

```bash
pio run
```

Expected: 编译成功，无错误。

- [ ] **Step 2: 提交（如编译通过）**

```bash
git commit --allow-empty -m "chore: 编译验证通过"
```

---

### Task 5: 上传设备验证

**Files:**
- 无文件修改

- [ ] **Step 1: 编译并上传**

```bash
pio run --target upload
```

Expected: 上传成功，设备重启后屏幕显示 `00:00:00` 白色点阵时钟，背景为暗淡圆点。

- [ ] **Step 2: 验证显示效果**

观察屏幕：
- 背景应有 48×27 个暗淡圆点均匀分布
- 白色点阵数字 `00:00:00` 应水平和垂直居中
- 数字应为 5×7 点阵，冒号为 1×4 点阵
- 无闪烁

- [ ] **Step 3: 最终提交**

```bash
git commit --allow-empty -m "feat: LED 点阵时钟上机验证通过"
```

---

## Self-Review

**1. Spec coverage:**
- ✅ 5×7 点阵字体（0-9 + A-Z）→ Task 1
- ✅ 1×4 冒号点阵 → Task 1
- ✅ 字符索引映射 → Task 1
- ✅ Sprite 双缓冲 → Task 2
- ✅ 背景暗点绘制 → Task 2
- ✅ 白色亮点绘制 → Task 2
- ✅ 居中布局（42 列宽，偏移 3 列/10 行）→ Task 2
- ✅ 默认显示 `00:00:00` → Task 3
- ✅ NTP 时间同步 → Task 3
- ✅ 纯白色亮点 → Task 2（`WHITE`）
- ✅ 无闪烁 → Task 2（Sprite 双缓冲）
- ✅ 保留 WiFi 配置服务 → Task 3

**2. Placeholder scan:** 无 TBD/TODO/"implement later"。每步都有完整代码。

**3. Type consistency:**
- `drawDot` 参数：`int16_t col, int16_t row, uint16_t color` — 与 M5GFX API 一致
- `drawChar` 参数：`int16_t startCol, int16_t startRow, const uint8_t mask[7]` — 与 FONT 维度匹配
- `drawColon` 参数：`int16_t startCol, int16_t startRow` — 与 COLON 维度匹配
- `drawClock` 参数：`const char* timeStr` — 与 `strftime` 输出一致
- 所有函数名和签名在各任务中一致。
