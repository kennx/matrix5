#include "orientation.h"

namespace {

bool resolveOrientation(float ax, float ay, ScreenOrientation fallback, ScreenOrientation& orientation) {
    float absAx = ax < 0 ? -ax : ax;
    float absAy = ay < 0 ? -ay : ay;

    if (absAx < OrientationManager::FLAT_THRESHOLD && absAy < OrientationManager::FLAT_THRESHOLD) {
        orientation = fallback;
        return false;
    }

    if (absAx == absAy) {
        orientation = fallback;
        return false;
    }

    orientation = (absAx > absAy) ? ScreenOrientation::Portrait : ScreenOrientation::Landscape;
    return true;
}

}  // namespace

void OrientationManager::begin(float ax, float ay) {
    ScreenOrientation initial = current_;
    resolveOrientation(ax, ay, current_, initial);
    current_ = initial;
    pending_ = initial;
    pendingSince_ = 0;
    justChanged_ = false;
}

void OrientationManager::update(float ax, float ay, unsigned long nowMs) {
    ScreenOrientation instant = current_;
    if (!resolveOrientation(ax, ay, current_, instant)) {
        pending_ = current_;
        justChanged_ = false;
        return;
    }

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
