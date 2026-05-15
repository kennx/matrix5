#include "config_store.h"
#include "config_types.h"
#include "../../../src/config_store.cpp"

#include <assert.h>

void test_device_config_roundtrip() {
    Preferences prefs;
    ConfigStore store(prefs);

    DeviceConfig noWifiNoNtp{"", "", "Asia/Shanghai", "", 60};
    assert(store.save(noWifiNoNtp));

    DeviceConfig loaded;
    assert(store.load(loaded));
    assert(loaded.wifiSsid.empty());
    assert(loaded.ntpServer.empty());
    assert(loaded.timezone == "Asia/Shanghai");
    assert(loaded.brightness == 60);
}

void test_battery_profile_roundtrip() {
    Preferences prefs;
    ConfigStore store(prefs);

    BatteryProfile profile;
    profile.learnedEmptyMv = 3390;
    profile.learnedFullMv = 4130;
    profile.curveBias = -3;
    profile.avgDischargeRate = 6.5f;
    profile.sampleCount = 4;
    profile.initialized = true;

    assert(store.saveBatteryProfile(profile));

    BatteryProfile loaded;
    assert(store.loadBatteryProfile(loaded));
    assert(loaded.learnedEmptyMv == profile.learnedEmptyMv);
    assert(loaded.learnedFullMv == profile.learnedFullMv);
    assert(loaded.curveBias == profile.curveBias);
    assert(loaded.avgDischargeRate == profile.avgDischargeRate);
    assert(loaded.sampleCount == profile.sampleCount);
    assert(loaded.initialized == profile.initialized);
}

void test_missing_battery_profile_returns_false() {
    Preferences prefs;
    ConfigStore store(prefs);

    BatteryProfile loaded;
    assert(!store.loadBatteryProfile(loaded));
    assert(loaded.initialized == false);
}

void test_battery_profile_with_inverted_range_returns_false() {
    Preferences prefs;
    ConfigStore store(prefs);

    BatteryProfile invalid;
    invalid.learnedEmptyMv = 4100;
    invalid.learnedFullMv = 3900;
    invalid.curveBias = 0;
    invalid.avgDischargeRate = 6.0f;
    invalid.sampleCount = 2;
    invalid.initialized = true;

    assert(store.saveBatteryProfile(invalid));

    BatteryProfile loaded;
    assert(!store.loadBatteryProfile(loaded));
    assert(loaded.initialized == false);
}

void test_obviously_corrupted_battery_profile_returns_false() {
    Preferences prefs;
    ConfigStore store(prefs);

    BatteryProfile corrupted;
    corrupted.learnedEmptyMv = 0;
    corrupted.learnedFullMv = 65535;
    corrupted.curveBias = 0;
    corrupted.avgDischargeRate = 6.0f;
    corrupted.sampleCount = 2;
    corrupted.initialized = true;

    assert(store.saveBatteryProfile(corrupted));

    BatteryProfile loaded;
    assert(!store.loadBatteryProfile(loaded));
    assert(loaded.initialized == false);
}

void test_battery_profile_with_negative_avg_discharge_rate_returns_false() {
    Preferences prefs;
    ConfigStore store(prefs);

    BatteryProfile invalid;
    invalid.learnedEmptyMv = 3390;
    invalid.learnedFullMv = 4130;
    invalid.curveBias = 0;
    invalid.avgDischargeRate = -1.0f;
    invalid.sampleCount = 2;
    invalid.initialized = true;

    assert(store.saveBatteryProfile(invalid));

    BatteryProfile loaded;
    assert(!store.loadBatteryProfile(loaded));
    assert(loaded.initialized == false);
}

void test_battery_profile_with_initialized_but_zero_samples_returns_false() {
    Preferences prefs;
    ConfigStore store(prefs);

    BatteryProfile invalid;
    invalid.learnedEmptyMv = 3390;
    invalid.learnedFullMv = 4130;
    invalid.curveBias = 0;
    invalid.avgDischargeRate = 6.0f;
    invalid.sampleCount = 0;
    invalid.initialized = true;

    assert(store.saveBatteryProfile(invalid));

    BatteryProfile loaded;
    assert(!store.loadBatteryProfile(loaded));
    assert(loaded.initialized == false);
}

int main() {
    test_device_config_roundtrip();
    test_battery_profile_roundtrip();
    test_missing_battery_profile_returns_false();
    test_battery_profile_with_inverted_range_returns_false();
    test_obviously_corrupted_battery_profile_returns_false();
    test_battery_profile_with_negative_avg_discharge_rate_returns_false();
    test_battery_profile_with_initialized_but_zero_samples_returns_false();
    return 0;
}
