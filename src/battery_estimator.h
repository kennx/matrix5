#pragma once

#include <cstdint>

struct BatteryProfile {
    uint16_t learnedEmptyMv = 3350;
    uint16_t learnedFullMv = 4100;
    int16_t curveBias = 0;
    float avgDischargeRate = 0.0f;
    uint16_t sampleCount = 0;
    bool initialized = false;
};

struct BatterySample {
    int voltageMv;
    bool charging;
    unsigned long timestampMs;
};

struct BatteryEstimate {
    int displayPercent;
    float baselinePercent;
    float runtimePercent;
    bool charging;
    bool usingLearnedProfile;
};

class BatteryEstimator {
public:
    static constexpr unsigned long SAMPLE_INTERVAL_MS = 5000;
    static constexpr unsigned long TRANSITION_SETTLE_MS = 90000;
    static constexpr unsigned long MIN_DISCHARGE_WINDOW_MS = 15UL * 60UL * 1000UL;

    void begin();
    void loadProfile(const BatteryProfile& profile);
    const BatteryProfile& profile() const;
    bool profileDirty() const;
    void clearProfileDirty();
    BatteryEstimate update(const BatterySample& sample);

private:
    static float baselinePercentForVoltage(int voltageMv, const BatteryProfile& profile);
    void maybeLearnFullAnchor(bool charging, unsigned long nowMs);
    void maybeLearnEmptyAnchor(bool charging, unsigned long nowMs);
    void maybeLearnDischargeRate(bool charging, unsigned long nowMs, float baselinePercent);

    BatteryProfile profile_;
    bool profileDirty_ = false;
    bool hasSample_ = false;
    float filteredVoltage_ = 0.0f;
    int displayPercent_ = -1;
    bool lastCharging_ = false;
    unsigned long lastTimestampMs_ = 0;
    unsigned long stateChangedAtMs_ = 0;
    int dischargeWindowStartMv_ = 0;
    float dischargeWindowStartPercent_ = 0.0f;
    unsigned long dischargeWindowStartMs_ = 0;
};
