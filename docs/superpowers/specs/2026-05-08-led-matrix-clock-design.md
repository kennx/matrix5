# LED 点阵时钟设计文档

## 1. 背景

matrix5 是一个基于 M5StickS3 (ESP32-S3) 的番茄钟项目，使用 ST7789P3 屏幕（135×240 分辨率）。当前已有一个 48×27 的 5×5 圆形点阵背景（`rgba(255,255,255,0.08)`）。本设计在此基础上叠加时钟显示。

## 2. 需求概述

- 在现有背景点阵上显示时间 `00:00:00`
- 数字/字母字符大小：25×35 像素（5×7 个大点）
- 冒号字符大小：5×20 像素（1×4 个大点）
- 每个"大点" = 5×5 像素，与背景圆形点完全对齐
- 字体等宽，范围覆盖 `0-9` 和 `A-Z`
- 点亮颜色：纯白色（`WHITE` / `0xFFFF`）
- 时钟显示水平和垂直居中
- 默认显示 `00:00:00`，支持 NTP 时间同步
- 预留符号扩展能力

## 3. 字体数据设计

### 3.1 数字与字母（5×7 点阵）

```
36 个字符：'0'-'9'(10个) + 'A'-'Z'(26个)
存储：uint8_t FONT[36][7]
每行 uint8_t 的低 5 位表示该行的 5 个点（bit4=最左列, bit0=最右列）
```

示例（数字 '0'）：
```
  点阵（5列×7行）：        十六进制存储（每行低5位）：
  0 1 1 1 0               0x0E
  1 0 0 0 1               0x11
  1 0 0 0 1               0x11
  1 0 0 0 1               0x11
  1 0 0 0 1               0x11
  1 0 0 0 1               0x11
  0 1 1 1 0               0x0E
```

### 3.2 冒号（1×4 点阵）

```
存储：uint8_t COLON[4]
两个点亮位置在相对行 1 和 3（中间隔一行）
```

### 3.3 字符索引映射

```cpp
int charIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;  // 扩展时在此添加新符号映射
}
```

## 4. 渲染架构

### 4.1 双缓冲

使用 `M5Canvas` 作为全屏帧缓冲：
- 尺寸：240×135 像素（与旋转后的屏幕一致）
- 色深：16 位（RGB565）
- 内存：240 × 135 × 2 ≈ 65KB，ESP32-S3 完全可承受

### 4.2 每帧绘制流程

```
1. sprite.fillSprite(BLACK)              // 清屏
2. 遍历 48×27 网格，画暗色圆点           // color565(20, 20, 20)
3. 遍历时间字符串，查字体表：
   - 字符 '0'-'9', 'A'-'Z' → 查 FONT[][]，在对应网格位置画 WHITE 圆点
   - 字符 ':' → 查 COLON[]，画 WHITE 圆点
4. sprite.pushSprite(0, 0)               // 整块推送到屏幕
```

屏幕永远只看到完整帧，零闪烁。

### 4.3 单点绘制

每个"大点"的绘制：
```cpp
int cx = col * DOT_SIZE + DOT_SIZE / 2;  // 列 → 像素中心 X
int cy = row * DOT_SIZE + DOT_SIZE / 2;  // 行 → 像素中心 Y
sprite.fillCircle(cx, cy, DOT_RADIUS, color);  // DOT_RADIUS = 2
```

## 5. 时钟显示布局

屏幕网格：48 列 × 27 行（每格 5×5 像素）

### 5.1 `00:00:00` 尺寸

| 项目 | 大点数 | 像素数 |
|------|--------|--------|
| 单个数字 | 5×7 | 25×35 |
| 单个冒号 | 1×4 | 5×20 |
| 8 个数字总宽 | 8×5 = 40 列 | 200 像素 |
| 2 个冒号总宽 | 2×1 = 2 列 | 10 像素 |
| **总宽度** | **42 列** | **210 像素** |
| **总高度** | **7 行** | **35 像素** |

字符间**不留额外间隙**（后续如需间距，需缩小字符宽度或调整布局）。

### 5.2 `00:00:00` 尺寸

| 项目 | 大点数 | 像素数 |
|------|--------|--------|
| 单个数字 | 5×7 | 25×35 |
| 单个冒号 | 1×4 | 5×20 |
| 8 个数字总宽 | 8×5 = 40 列 | 200 像素 |
| 2 个冒号总宽 | 2×1 = 2 列 | 10 像素 |
| **总宽度** | **42 列** | **210 像素** |
| **总高度** | **7 行** | **35 像素** |

### 5.3 居中计算

```
水平偏移 = (48 - 42) / 2 = 3 列 = 15 像素
垂直偏移 = (27 - 7) / 2 = 10 行 = 50 像素
```

时钟显示区域起始于屏幕像素坐标 `(15, 50)`。

### 5.3 冒号垂直对齐

- 数字占 7 行（相对行 0~6）
- 冒号占 4 行，垂直居中于数字高度内
- 冒号起始相对行 = (7 - 4) / 2 = 1
- 冒号占据相对行 1, 2, 3, 4
- **两个点亮位置在相对行 1 和 3**（绝对行 12 和 14）

## 6. 主程序流程

### 6.1 setup()

```cpp
void setup() {
    M5.begin(cfg);
    Serial.begin(115200);
    M5.Display.setRotation(1);
    
    // 创建双缓冲 sprite
    sprite.setColorDepth(16);
    sprite.createSprite(M5.Display.width(), M5.Display.height());
    
    // 绘制默认时间
    drawClock("00:00:00");
    
    // WiFi 配置（保留原有逻辑）
    prefs.begin("m5stick", false);
    if (!tryConnectWiFi()) {
        startAPMode();
    }
}
```

### 6.2 loop()

```cpp
void loop() {
    if (apMode) {
        server.handleClient();
    } else {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate >= 1000) {
            lastUpdate = millis();
            
            // 获取当前时间
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

### 6.3 drawClock()

```cpp
void drawClock(const char* timeStr) {
    sprite.fillSprite(BLACK);
    
    // 1. 画背景暗点（48×27）
    drawBackgroundDots();
    
    // 2. 画时间亮点（字符紧密排列，无间距）
    int startCol = 3;   // 水平居中偏移 (48-42)/2 = 3
    int startRow = 10;  // 垂直居中偏移
    int col = startCol;
    
    for (const char* p = timeStr; *p; p++) {
        if (*p == ':') {
            drawColon(col, startRow + 1);  // 冒号垂直居中于数字内
            col += 1;       // 冒号占 1 列
        } else {
            int idx = charIndex(*p);
            if (idx >= 0) {
                drawChar(col, startRow, FONT[idx]);
            }
            col += 5;       // 数字占 5 列
        }
    }
    
    sprite.pushSprite(0, 0);
}
```

## 7. 扩展性

- **添加新符号**：在 `FONT` 数组末尾追加点阵数据，在 `charIndex()` 中添加映射即可
- **修改颜色**：将 `WHITE` 替换为其他颜色常量，或支持根据状态切换颜色（比如番茄钟倒计时变红）
- **动画效果**：由于是全量重绘，可以方便地实现数字滚动、闪烁等效果
- **不同字号**：当前 5×7 是固定的，如需更小/更大的字符，需要重新定义字体数据和字符宽度

## 8. 性能预估

- 每帧绘制：1296 个暗点 + 约 60 个亮点（`00:00:00` 约 8×35 + 2×2 = 284 个大点，但每帧通常只变 1-2 个数字）
- `fillCircle(r=2)` 实际只写约 4-5 个像素，总像素操作约 6000-7000 次
- ESP32-S3 在 240MHz 下，加上 SPI 推送，每帧耗时远小于 16ms（60fps）
- 每秒更新一次，性能完全充裕
