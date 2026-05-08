#include <M5Unified.h>

static const char* DEFAULT_TIME = "12:34:56";

// 5×7 LED 点阵掩码，每行 5 位（高位在左），7 行从上到下
static const uint8_t LED_MASKS[11][7] = {
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
    {0x00, 0x00, 0x04, 0x00, 0x00, 0x04, 0x00}, // : 单像素居中
};

static const uint8_t DOT_PITCH = 4;   // 相邻圆点中心距离
static const uint8_t DOT_RADIUS = 0;  // 单像素圆点
static const uint8_t COLS = 5;
static const uint8_t ROWS = 7;
static const uint8_t CHAR_GAP = 2;    // 字符之间额外间隙

static inline int ledIndex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch == ':') return 10;
    return -1;
}

static void drawLEDChar(int16_t x, int16_t y, char ch, uint16_t color) {
    int idx = ledIndex(ch);
    if (idx < 0) return;

    const uint8_t* mask = LED_MASKS[idx];
    for (int row = 0; row < ROWS; row++) {
        uint8_t bits = mask[row];
        for (int col = 0; col < COLS; col++) {
            if (bits & (1 << (4 - col))) {
                int cx = x + col * DOT_PITCH + DOT_PITCH / 2;
                int cy = y + row * DOT_PITCH + DOT_PITCH / 2;
                M5.Display.drawPixel(cx, cy, color);
            }
        }
    }
}

static void drawLEDString(int16_t x, int16_t y, const char* str, uint16_t color) {
    while (*str) {
        drawLEDChar(x, y, *str, color);
        x += COLS * DOT_PITCH + CHAR_GAP;
        str++;
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);

    int16_t charW = COLS * DOT_PITCH;
    int16_t charH = ROWS * DOT_PITCH;
    int16_t len = strlen(DEFAULT_TIME);
    int16_t textW = len * charW + (len - 1) * CHAR_GAP;
    int16_t textH = charH;

    int16_t x = (M5.Display.width()  - textW) / 2;
    int16_t y = (M5.Display.height() - textH) / 2;

    drawLEDString(x, y, DEFAULT_TIME, WHITE);
}

void loop() {
    M5.delay(100);
}
