#include "display_logic.h"

#include <cstddef>

namespace {

int textWidthInCols(std::size_t len) {
    if (len == 0) {
        return 0;
    }
    return static_cast<int>(len * 5 + (len - 1));
}

}  // namespace

bool shouldRenderPeriodicFrame(unsigned long nowMs, unsigned long lastUpdateMs, unsigned long intervalMs, bool forceRedraw) {
    return forceRedraw || nowMs - lastUpdateMs >= intervalMs;
}

int choosePortraitTextSplit(const char* text, int maxCols) {
    std::size_t len = 0;
    while (text[len] != '\0') {
        len++;
    }

    if (textWidthInCols(len) <= maxCols) {
        return 0;
    }

    std::size_t split = (len + 1) / 2;
    if (split == 0 || split >= len) {
        return 0;
    }

    if (textWidthInCols(split) > maxCols || textWidthInCols(len - split) > maxCols) {
        return 0;
    }

    return static_cast<int>(split);
}
