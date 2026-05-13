#pragma once

#include <cstdint>

enum class ScreenOrientation { Landscape, Portrait };

class OrientationManager {
public:
    void update(float ax, float ay, unsigned long nowMs);

    ScreenOrientation getOrientation() const { return current_; }
    bool orientationJustChanged() const { return justChanged_; }

    static constexpr unsigned long DEBOUNCE_MS = 500;
    static constexpr float FLAT_THRESHOLD = 0.15f;

private:
    ScreenOrientation current_ = ScreenOrientation::Portrait;
    ScreenOrientation pending_ = ScreenOrientation::Portrait;
    unsigned long pendingSince_ = 0;
    bool justChanged_ = false;
};
