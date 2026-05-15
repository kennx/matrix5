#include "battery_estimator.h"

#include <cmath>

namespace {

struct CurvePoint {
    int mv;
    float percent;
};

constexpr CurvePoint kBaselineCurve[] = {
    {3350, 0.0f},
    {3500, 5.0f},
    {3600, 12.0f},
    {3700, 24.0f},
    {3800, 42.0f},
    {3900, 60.0f},
    {4000, 78.0f},
    {4100, 92.0f},
    {4180, 100.0f},
};

constexpr int kLearnFullMinMv = 4080;
constexpr int kLearnEmptyMaxMv = 3500;
constexpr float kDefaultDischargeRate = 8.0f;

constexpr int kMaxBacklightDropMv = 40;
constexpr int kWifiDropMv = 50;

float clampPercent(float percent) {
    if (percent < 0.0f) {
        return 0.0f;
    }
    if (percent > 100.0f) {
        return 100.0f;
    }
    return percent;
}

uint16_t blendMv(uint16_t previous, int current) {
    return static_cast<uint16_t>((static_cast<unsigned int>(previous) * 4U +
                                  static_cast<unsigned int>(current)) /
                                 5U);
}

int shiftMvForPercent(float percent, const BatteryProfile& profile) {
    if (!profile.initialized) {
        return 0;
    }

    const float ratio = clampPercent(percent) / 100.0f;
    const float emptyShift = static_cast<float>(static_cast<int>(profile.learnedEmptyMv) - 3350);
    const float fullShift = static_cast<float>(static_cast<int>(profile.learnedFullMv) - 4100);
    const float shift = emptyShift + (fullShift - emptyShift) * ratio;
    return static_cast<int>(std::lround(shift));
}

CurvePoint adjustedMv(const CurvePoint& point, const BatteryProfile& profile) {
    CurvePoint adjusted = point;
    adjusted.mv += shiftMvForPercent(point.percent, profile);
    return adjusted;
}

}  // namespace

void BatteryEstimator::begin() {
    profileDirty_ = false;
    hasSample_ = false;
    filteredVoltage_ = 0.0f;
    displayPercent_ = -1;
    lastCharging_ = false;
    lastTimestampMs_ = 0;
    stateChangedAtMs_ = 0;
    dischargeWindowStartMv_ = 0;
    dischargeWindowStartPercent_ = 0.0f;
    dischargeWindowStartMs_ = 0;
}

void BatteryEstimator::loadProfile(const BatteryProfile& profile) {
    profile_ = profile;
    profileDirty_ = false;
}

const BatteryProfile& BatteryEstimator::profile() const {
    return profile_;
}

bool BatteryEstimator::profileDirty() const {
    return profileDirty_;
}

void BatteryEstimator::clearProfileDirty() {
    profileDirty_ = false;
}

float BatteryEstimator::baselinePercentForVoltage(int voltageMv, const BatteryProfile& profile) {
    const CurvePoint first = adjustedMv(kBaselineCurve[0], profile);
    if (voltageMv <= first.mv) {
        return 0.0f;
    }

    constexpr size_t kPointCount = sizeof(kBaselineCurve) / sizeof(kBaselineCurve[0]);
    const CurvePoint last = adjustedMv(kBaselineCurve[kPointCount - 1], profile);
    if (voltageMv >= last.mv) {
        return 100.0f;
    }

    for (size_t i = 1; i < kPointCount; ++i) {
        const CurvePoint lower = adjustedMv(kBaselineCurve[i - 1], profile);
        const CurvePoint upper = adjustedMv(kBaselineCurve[i], profile);
        if (voltageMv <= upper.mv) {
            const float rangeMv = static_cast<float>(upper.mv - lower.mv);
            const float ratio = rangeMv <= 0.0f
                                    ? 0.0f
                                    : static_cast<float>(voltageMv - lower.mv) / rangeMv;
            const float percent = lower.percent + (upper.percent - lower.percent) * ratio;
            return clampPercent(percent);
        }
    }

    return 100.0f;
}

void BatteryEstimator::maybeLearnFullAnchor(bool charging, unsigned long nowMs) {
    if (!charging || nowMs - stateChangedAtMs_ < TRANSITION_SETTLE_MS) {
        return;
    }

    const int currentMv = static_cast<int>(std::lround(filteredVoltage_));
    if (currentMv < kLearnFullMinMv) {
        return;
    }

    const uint16_t learned = blendMv(profile_.learnedFullMv, currentMv);
    if (profile_.initialized &&
        std::abs(static_cast<int>(learned) - static_cast<int>(profile_.learnedFullMv)) < 2) {
        return;
    }

    profile_.learnedFullMv = learned;
    profile_.initialized = true;
    if (profile_.sampleCount < 0xFFFF) {
        ++profile_.sampleCount;
    }
    profileDirty_ = true;
}

void BatteryEstimator::maybeLearnEmptyAnchor(bool charging, unsigned long nowMs) {
    if (charging || nowMs - stateChangedAtMs_ < TRANSITION_SETTLE_MS) {
        return;
    }

    const int currentMv = static_cast<int>(std::lround(filteredVoltage_));
    if (currentMv > kLearnEmptyMaxMv) {
        return;
    }

    const uint16_t learned = blendMv(profile_.learnedEmptyMv, currentMv);
    if (profile_.initialized &&
        std::abs(static_cast<int>(learned) - static_cast<int>(profile_.learnedEmptyMv)) < 2) {
        return;
    }

    profile_.learnedEmptyMv = learned;
    profile_.initialized = true;
    if (profile_.sampleCount < 0xFFFF) {
        ++profile_.sampleCount;
    }
    profileDirty_ = true;
}

void BatteryEstimator::maybeLearnDischargeRate(bool charging,
                                               unsigned long nowMs,
                                               float baselinePercent) {
    const int currentMv = static_cast<int>(std::lround(filteredVoltage_));

    if (charging) {
        dischargeWindowStartMs_ = 0;
        dischargeWindowStartMv_ = 0;
        dischargeWindowStartPercent_ = 0.0f;
        return;
    }

    if (nowMs - stateChangedAtMs_ < TRANSITION_SETTLE_MS) {
        dischargeWindowStartMs_ = 0;
        dischargeWindowStartMv_ = 0;
        dischargeWindowStartPercent_ = 0.0f;
        return;
    }

    if (dischargeWindowStartMs_ == 0 && dischargeWindowStartMv_ == 0) {
        dischargeWindowStartMs_ = nowMs;
        dischargeWindowStartMv_ = currentMv;
        dischargeWindowStartPercent_ = baselinePercent;
        return;
    }

    const unsigned long elapsed = nowMs - dischargeWindowStartMs_;
    const float deltaPercent = dischargeWindowStartPercent_ - baselinePercent;
    const int deltaMv = dischargeWindowStartMv_ - currentMv;
    if (elapsed < MIN_DISCHARGE_WINDOW_MS || deltaPercent < 4.0f || deltaMv < 30) {
        return;
    }

    const float hours = static_cast<float>(elapsed) / 3600000.0f;
    const float rate = deltaPercent / hours;
    if (rate <= 0.0f || rate > 30.0f) {
        dischargeWindowStartMs_ = nowMs;
        dischargeWindowStartMv_ = currentMv;
        dischargeWindowStartPercent_ = baselinePercent;
        return;
    }

    const float previous = profile_.avgDischargeRate > 0.0f ? profile_.avgDischargeRate
                                                             : kDefaultDischargeRate;
    profile_.avgDischargeRate = previous * 0.8f + rate * 0.2f;

    float bias = (kDefaultDischargeRate - profile_.avgDischargeRate) * 0.8f;
    if (bias < -10.0f) {
        bias = -10.0f;
    }
    if (bias > 10.0f) {
        bias = 10.0f;
    }

    profile_.curveBias = static_cast<int16_t>(std::lround(bias));
    profile_.initialized = true;
    if (profile_.sampleCount < 0xFFFF) {
        ++profile_.sampleCount;
    }
    profileDirty_ = true;

    dischargeWindowStartMs_ = nowMs;
    dischargeWindowStartMv_ = currentMv;
    dischargeWindowStartPercent_ = baselinePercent;
}

BatteryEstimate BatteryEstimator::update(const BatterySample& sample) {
    const int compensatedMv = sample.voltageMv 
        + (sample.backlightPercent * kMaxBacklightDropMv / 100)
        + (sample.wifiActive ? kWifiDropMv : 0);

    if (!hasSample_) {
        hasSample_ = true;
        filteredVoltage_ = static_cast<float>(compensatedMv);

        const float baseline = baselinePercentForVoltage(compensatedMv, profile_);
        const int display = static_cast<int>(std::lround(baseline));

        displayPercent_ = display;
        lastCharging_ = sample.charging;
        lastTimestampMs_ = sample.timestampMs;
        stateChangedAtMs_ = sample.timestampMs;
        dischargeWindowStartMv_ = compensatedMv;
        dischargeWindowStartPercent_ = baseline;
        dischargeWindowStartMs_ = sample.timestampMs;

        return {display,
                baseline,
                baseline,
                sample.charging,
                profile_.initialized && profile_.sampleCount > 0};
    }

    if (sample.charging != lastCharging_) {
        stateChangedAtMs_ = sample.timestampMs;
    }

    const unsigned long elapsedSinceLast = sample.timestampMs - lastTimestampMs_;
    if (elapsedSinceLast > SAMPLE_INTERVAL_MS) {
        filteredVoltage_ = static_cast<float>(compensatedMv);
    } else {
        filteredVoltage_ = filteredVoltage_ * 0.80f + static_cast<float>(compensatedMv) * 0.20f;
    }

    const float baseline =
        baselinePercentForVoltage(static_cast<int>(std::lround(filteredVoltage_)), profile_);
    maybeLearnFullAnchor(sample.charging, sample.timestampMs);
    maybeLearnEmptyAnchor(sample.charging, sample.timestampMs);
    maybeLearnDischargeRate(sample.charging, sample.timestampMs, baseline);

    float runtime = baseline;
    if (profile_.initialized && profile_.sampleCount > 0) {
        const float biased = clampPercent(baseline + static_cast<float>(profile_.curveBias));
        const float weight = profile_.sampleCount >= 3
                                 ? 1.0f
                                 : static_cast<float>(profile_.sampleCount) / 3.0f;
        runtime = baseline + (biased - baseline) * weight;
    }

    float candidate = runtime;
    const unsigned long settleElapsed = sample.timestampMs - stateChangedAtMs_;

    if (settleElapsed < TRANSITION_SETTLE_MS) {
        const float progress = static_cast<float>(settleElapsed) /
                               static_cast<float>(TRANSITION_SETTLE_MS);
        candidate = static_cast<float>(displayPercent_) +
                    (candidate - static_cast<float>(displayPercent_)) * progress;
    }

    if (sample.charging && candidate < static_cast<float>(displayPercent_)) {
        candidate = static_cast<float>(displayPercent_);
    }

    if (!sample.charging && candidate > static_cast<float>(displayPercent_)) {
        candidate = static_cast<float>(displayPercent_);
    }

    int rounded = static_cast<int>(std::lround(candidate));
    if (std::abs(rounded - displayPercent_) <= 1) {
        rounded = displayPercent_;
    }

    displayPercent_ = rounded;
    lastCharging_ = sample.charging;
    lastTimestampMs_ = sample.timestampMs;

    return {displayPercent_,
            baseline,
            runtime,
            sample.charging,
            profile_.initialized && profile_.sampleCount > 0};
}
