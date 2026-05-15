#include "battery_estimator.h"
#include "../../../src/battery_estimator.cpp"

#include <cassert>

static BatterySample sample(int mv, bool charging, unsigned long ts, uint8_t backlight = 0, bool wifi = false) {
    return {mv, charging, ts, backlight, wifi};
}

static void test_baseline_curve_3900mv_is_midrange() {
    BatteryEstimator estimator;
    estimator.begin();

    const BatteryEstimate estimate = estimator.update(sample(3900, false, 0));

    assert(estimate.displayPercent >= 58);
    assert(estimate.displayPercent <= 62);
    assert(!estimate.usingLearnedProfile);
}

static void test_baseline_clamps_at_bounds() {
    BatteryEstimator estimator;
    estimator.begin();

    const BatteryEstimate low = estimator.update(sample(3200, false, 0));
    assert(low.displayPercent == 0);

    estimator.begin();
    const BatteryEstimate high = estimator.update(sample(4200, false, 0));
    assert(high.displayPercent == 100);
}

static void test_loaded_profile_shifts_same_voltage_lower() {
    BatteryEstimator estimator;
    estimator.begin();

    const BatteryEstimate baselineEstimate = estimator.update(sample(3900, false, 0));
    const int baseline = baselineEstimate.displayPercent;

    estimator.begin();
    estimator.loadProfile({3400, 4200, 0, 0.0f, 0, true});
    const BatteryEstimate learnedEstimate = estimator.update(sample(3900, false, 0));

    assert(learnedEstimate.displayPercent < baseline);
}

static void test_discharging_noise_does_not_raise_display() {
    BatteryEstimator estimator;
    estimator.begin();

    const BatteryEstimate first = estimator.update(sample(3920, false, 0));
    const BatteryEstimate second = estimator.update(sample(3990, false, 5000));

    assert(second.displayPercent == first.displayPercent);
}

static void test_charging_noise_does_not_drop_display() {
    BatteryEstimator estimator;
    estimator.begin();

    const BatteryEstimate first = estimator.update(sample(3850, true, 0));
    const BatteryEstimate second = estimator.update(sample(3770, true, 5000));

    assert(second.displayPercent == first.displayPercent);
}

static void test_unplug_transition_delays_large_drop() {
    BatteryEstimator estimator;
    estimator.begin();

    const BatteryEstimate plugged = estimator.update(sample(4100, true, 0));
    const BatteryEstimate justUnplugged = estimator.update(sample(3980, false, 5000));
    const BatteryEstimate midTransition = estimator.update(sample(3980, false, 50000));
    const BatteryEstimate settled = estimator.update(sample(3980, false, 100000));

    assert(justUnplugged.displayPercent >= plugged.displayPercent - 1);
    assert(midTransition.displayPercent < plugged.displayPercent);
    assert(midTransition.displayPercent > settled.displayPercent);
    assert(settled.displayPercent < plugged.displayPercent);
}

static void test_sustained_charge_learns_full_anchor() {
    BatteryEstimator estimator;
    estimator.begin();

    estimator.update(sample(4100, true, 0));
    estimator.clearProfileDirty();
    estimator.update(sample(4110, true, 100000));

    assert(estimator.profileDirty());
    assert(estimator.profile().initialized);
    assert(estimator.profile().learnedFullMv >= 4102);
}

static void test_low_voltage_learns_empty_anchor() {
    BatteryEstimator estimator;
    estimator.begin();

    estimator.update(sample(3480, false, 0));
    estimator.clearProfileDirty();
    estimator.update(sample(3470, false, 100000));

    assert(estimator.profileDirty());
    assert(estimator.profile().initialized);
    assert(estimator.profile().learnedEmptyMv <= 3478);
}

static void test_long_discharge_updates_rate_and_bias() {
    BatteryEstimator estimator;
    estimator.begin();

    estimator.update(sample(4020, false, 0));
    estimator.clearProfileDirty();
    const BatteryEstimate learned = estimator.update(sample(3970, false, 20UL * 60UL * 1000UL));

    assert(estimator.profileDirty());
    assert(estimator.profile().avgDischargeRate > 0.0f);
    assert(estimator.profile().curveBias < 0);
    assert(learned.runtimePercent < learned.baselinePercent);
    assert(learned.usingLearnedProfile == true);
}

static void test_unplug_transition_does_not_start_discharge_learning() {
    BatteryEstimator estimator;
    estimator.begin();
    estimator.loadProfile({2500, 4200, 0, 0.0f, 1, true});

    estimator.update(sample(4100, true, 0));
    estimator.clearProfileDirty();
    estimator.update(sample(3980, false, 5000));
    assert(!estimator.profileDirty());
    assert(estimator.profile().avgDischargeRate == 0.0f);
    estimator.update(sample(3940, false, 20UL * 60UL * 1000UL));

    assert(!estimator.profileDirty());
    assert(estimator.profile().avgDischargeRate == 0.0f);
}

static void test_load_compensation_recovers_ocv() {
    BatteryEstimator estimator;
    estimator.begin();

    const BatteryEstimate baseline = estimator.update(sample(3900, false, 0, 0, false));
    const BatteryEstimate loaded = estimator.update(sample(3810, false, 5000, 100, true));
    
    assert(loaded.baselinePercent == baseline.baselinePercent);
}

int main() {
    test_baseline_curve_3900mv_is_midrange();
    test_baseline_clamps_at_bounds();
    test_loaded_profile_shifts_same_voltage_lower();
    test_discharging_noise_does_not_raise_display();
    test_charging_noise_does_not_drop_display();
    test_unplug_transition_delays_large_drop();
    test_sustained_charge_learns_full_anchor();
    test_low_voltage_learns_empty_anchor();
    test_long_discharge_updates_rate_and_bias();
    test_unplug_transition_does_not_start_discharge_learning();
    test_load_compensation_recovers_ocv();
    return 0;
}
