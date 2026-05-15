# Battery Load Compensation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compensate battery ADC voltage readings for internal resistance voltage drop under variable system loads (screen brightness and WiFi activity) to provide stable OCV-based battery estimates.

**Architecture:** Extend the `BatterySample` struct with load state, calculate a compensated voltage upon receiving the sample, and use this compensated voltage for all subsequent filtering and state machine logic inside `BatteryEstimator`.

**Tech Stack:** C++, ESP32/Arduino, M5Unified

---

### Task 1: Extend BatterySample and Constants

**Files:**
- Modify: `src/battery_estimator.h`
- Modify: `src/battery_estimator.cpp`

- [ ] **Step 1: Write the changes to battery_estimator.h**

```cpp
struct BatterySample {
    int voltageMv;
    bool charging;
    unsigned long timestampMs;
    uint8_t backlightPercent; // 0-100
    bool wifiActive;
};
```

- [ ] **Step 2: Add compensation constants to battery_estimator.cpp**
Add these to the anonymous namespace in `src/battery_estimator.cpp`:
```cpp
constexpr int kMaxBacklightDropMv = 40;
constexpr int kWifiDropMv = 50;
```

- [ ] **Step 3: Commit**
```bash
git add src/battery_estimator.h src/battery_estimator.cpp
git commit -m "feat(battery): extend BatterySample for load compensation"
```

### Task 2: Update Tests for New Struct

**Files:**
- Modify: `test/native/test_battery_estimator/test_main.cpp`

- [ ] **Step 1: Update the `sample` helper function**
Update the test helper in `test_main.cpp` to fill the new fields (with defaults for existing tests).

```cpp
static BatterySample sample(int mv, bool charging, unsigned long ts, uint8_t backlight = 0, bool wifi = false) {
    return {mv, charging, ts, backlight, wifi};
}
```

- [ ] **Step 2: Run tests to verify they still pass**
Run: `pio test -e native`
Expected: PASS

- [ ] **Step 3: Commit**
```bash
git add test/native/test_battery_estimator/test_main.cpp
git commit -m "test(battery): adapt tests to new BatterySample struct"
```

### Task 3: Implement Compensation Logic

**Files:**
- Modify: `src/battery_estimator.cpp`
- Modify: `test/native/test_battery_estimator/test_main.cpp`

- [ ] **Step 1: Write the failing test for compensation**
Add this test to `test_main.cpp` and register it in `main()`:
```cpp
static void test_load_compensation_recovers_ocv() {
    BatteryEstimator estimator;
    estimator.begin();

    // 1. Baseline reading, no load
    const BatteryEstimate baseline = estimator.update(sample(3900, false, 0, 0, false));
    
    // 2. High load reading, voltage drops, but compensation should recover it
    // Screen 100% -> +40mV compensation
    // WiFi active -> +50mV compensation
    // Total compensation = 90mV.
    // If raw is 3810, compensated is 3900.
    // Display percent shouldn't drop due to the state machine, but we can check baseline.
    const BatteryEstimate loaded = estimator.update(sample(3810, false, 5000, 100, true));
    
    // The underlying baseline percent should be completely identical because 3810 + 90 = 3900
    assert(loaded.baselinePercent == baseline.baselinePercent);
}
```

- [ ] **Step 2: Run test to verify it fails**
Run: `pio test -e native`
Expected: FAIL on `test_load_compensation_recovers_ocv`

- [ ] **Step 3: Implement compensation in BatteryEstimator::update**
Modify `BatteryEstimator::update(const BatterySample& sample)` to compute compensated voltage:

```cpp
BatteryEstimate BatteryEstimator::update(const BatterySample& sample) {
    // Add these lines at the beginning of the function
    const int compensatedMv = sample.voltageMv 
        + (sample.backlightPercent * kMaxBacklightDropMv / 100)
        + (sample.wifiActive ? kWifiDropMv : 0);

    if (!hasSample_) {
        hasSample_ = true;
        filteredVoltage_ = static_cast<float>(compensatedMv);

        const float baseline = baselinePercentForVoltage(compensatedMv, profile_);
        // ... rest of the first-sample logic, replacing sample.voltageMv with compensatedMv for internal states
        // BE SURE to change dischargeWindowStartMv_ to compensatedMv
```
Also update the `else` branch of `hasSample_`:
```cpp
    const unsigned long elapsedSinceLast = sample.timestampMs - lastTimestampMs_;
    if (elapsedSinceLast > SAMPLE_INTERVAL_MS) {
        filteredVoltage_ = static_cast<float>(compensatedMv);
    } else {
        filteredVoltage_ = filteredVoltage_ * 0.80f + static_cast<float>(compensatedMv) * 0.20f;
    }
```
*Note: Make sure to replace all relevant `sample.voltageMv` usages that relate to baseline/learning (like `dischargeWindowStartMv_`) with `compensatedMv`. The returned struct doesn't contain voltage, so nothing to change there.*

- [ ] **Step 4: Run test to verify it passes**
Run: `pio test -e native`
Expected: PASS

- [ ] **Step 5: Commit**
```bash
git add src/battery_estimator.cpp test/native/test_battery_estimator/test_main.cpp
git commit -m "feat(battery): implement voltage drop compensation for load"
```

### Task 4: Integrate in main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Write integration in main.cpp**
In `src/main.cpp`, modify `refreshBatteryEstimate`:

```cpp
static void refreshBatteryEstimate(unsigned long nowMs) {
    static unsigned long lastBatterySample = 0;
    if (hasBatteryEstimate && nowMs - lastBatterySample < BatteryEstimator::SAMPLE_INTERVAL_MS) {
        return;
    }

    uint8_t currentBrightness = 50; // Default or read from pendingConfig/cfgData
    // We can read it from configStore if we don't have it globally, or just use M5.Display.getBrightness().
    // Actually, M5.Display.getBrightness() returns 0-255. We need 0-100.
    currentBrightness = (M5.Display.getBrightness() * 100) / 255;
    
    bool isWifiActive = (WiFi.getMode() == WIFI_STA) && (WiFi.status() == WL_CONNECTED || wifiScanInProgress);

    BatterySample sample{M5.Power.getBatteryVoltage(), isBatteryCharging(), nowMs, currentBrightness, isWifiActive};
    lastBatteryEstimate = batteryEstimator.update(sample);
    hasBatteryEstimate = true;
    lastBatterySample = nowMs;

// ...
```

- [ ] **Step 2: Compile to verify**
Run: `pio run -e m5sticks3`
Expected: SUCCESS

- [ ] **Step 3: Commit**
```bash
git add src/main.cpp
git commit -m "feat(battery): provide load state to battery estimator in main"
```
