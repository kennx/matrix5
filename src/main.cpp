#include <M5Unified.h>
#include "fonts/doto_36.h"

static const char* DEFAULT_TIME = "12:34:56";

// 将 8bpp 灰度位图混合为 RGB565 后推送到屏幕
static void drawGlyph(int16_t x, int16_t y, const Glyph& g, uint16_t color) {
    if (g.w == 0 || g.h == 0) return;

    uint8_t fg_r = (color >> 11) & 0x1F;
    uint8_t fg_g = (color >> 5)  & 0x3F;
    uint8_t fg_b =  color        & 0x1F;

    // 最大字符 22×24 = 528 像素 → 1056 字节，栈上安全
    uint16_t buf[22 * 24];

    for (int i = 0; i < g.w * g.h; i++) {
        uint8_t gray = g.bitmap[i];
        if (gray == 0) {
            buf[i] = 0x0000;                     // 纯黑背景
        } else if (gray == 255) {
            buf[i] = color;                        // 纯色前景
        } else {
            uint8_t r = (fg_r * gray) >> 8;
            uint8_t gv = (fg_g * gray) >> 8;
            uint8_t b = (fg_b * gray) >> 8;
            buf[i] = (r << 11) | (gv << 5) | b;
        }
    }

    M5.Display.pushImage(x, y, g.w, g.h, buf);
}

static void drawDotoString(int16_t x, int16_t y, const char* str, uint16_t color) {
    while (*str) {
        int idx = dotoCharIndex(*str);
        if (idx >= 0) {
            const Glyph& g = dotoGlyphs[idx];
            drawGlyph(x + g.xOffset, y + g.yOffset, g, color);
            x += g.advance;
        }
        str++;
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);

    // 计算文本在 240×135 屏幕上的居中位置
    int16_t textW = strlen(DEFAULT_TIME) * dotoAdvance;   // 8 × 22 = 176
    int16_t textH = dotoAscent + dotoDescent;              // 35 + 9 = 44（字体总高）

    int16_t baselineX = (M5.Display.width()  - textW) / 2;
    int16_t baselineY = (M5.Display.height() - textH) / 2 + dotoAscent;

    drawDotoString(baselineX, baselineY, DEFAULT_TIME, WHITE);
}

void loop() {
    M5.delay(100);
}
