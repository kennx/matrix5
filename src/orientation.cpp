#include "orientation.h"

void OrientationManager::update(float ax, float ay, unsigned long nowMs) {
    float absAx = ax < 0 ? -ax : ax;
    float absAy = ay < 0 ? -ay : ay;

    // 平放检测：X/Y 平面没有显著重力分量时不改变方向
    if (absAx < FLAT_THRESHOLD && absAy < FLAT_THRESHOLD) {
        pending_ = current_;
        justChanged_ = false;
        return;
    }

    ScreenOrientation instant = (absAx >= absAy) ? ScreenOrientation::Portrait : ScreenOrientation::Landscape;

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
