#include "orientation.h"

static ScreenOrientation orientationFromAccel(float ax, float ay) {
    if (ax < 0) ax = -ax;
    if (ay < 0) ay = -ay;
    return (ax >= ay) ? ScreenOrientation::Landscape : ScreenOrientation::Portrait;
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
