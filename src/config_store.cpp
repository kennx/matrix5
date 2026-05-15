#include "config_store.h"

namespace {

constexpr uint16_t kMinBatteryProfileMv = 3000;
constexpr uint16_t kMaxBatteryProfileMv = 4300;
constexpr int16_t kMinCurveBias = -10;
constexpr int16_t kMaxCurveBias = 10;

bool isValidBatteryProfile(const BatteryProfile& profile) {
    if (profile.learnedEmptyMv < kMinBatteryProfileMv || profile.learnedEmptyMv > kMaxBatteryProfileMv) {
        return false;
    }

    if (profile.learnedFullMv < kMinBatteryProfileMv || profile.learnedFullMv > kMaxBatteryProfileMv) {
        return false;
    }

    if (profile.learnedFullMv <= profile.learnedEmptyMv) {
        return false;
    }

    if (profile.avgDischargeRate < 0.0f) {
        return false;
    }

    if (profile.curveBias < kMinCurveBias || profile.curveBias > kMaxCurveBias) {
        return false;
    }

    if (profile.initialized && profile.sampleCount == 0) {
        return false;
    }

    return true;
}

}  // namespace

bool ConfigStore::load(DeviceConfig& outConfig) {
    outConfig.wifiSsid = prefs_.getString("ssid", "").c_str();
    outConfig.wifiPassword = prefs_.getString("password", "").c_str();
    outConfig.timezone = prefs_.getString("timezone", "Asia/Shanghai").c_str();
    outConfig.ntpServer = prefs_.getString("ntp_server", "pool.ntp.org").c_str();
    outConfig.brightness = prefs_.getUChar("brightness", 50);
    return prefs_.isKey("timezone") && prefs_.isKey("ntp_server");
}

bool ConfigStore::save(const DeviceConfig& config) {
    bool ok = true;
    ok = ok && prefs_.putString("ssid", config.wifiSsid.c_str()) >= 0;
    ok = ok && prefs_.putString("password", config.wifiPassword.c_str()) >= 0;
    ok = ok && prefs_.putString("timezone", config.timezone.c_str()) > 0;
    ok = ok && (prefs_.putString("ntp_server", config.ntpServer.c_str()) > 0 || config.ntpServer.empty());
    ok = ok && prefs_.putUChar("brightness", config.brightness) > 0;
    return ok;
}

bool ConfigStore::loadBatteryProfile(BatteryProfile& outProfile) {
    outProfile = BatteryProfile{};
    if (!prefs_.getBool("bat_init", false)) {
        return false;
    }

    outProfile.learnedEmptyMv = prefs_.getUShort("bat_empty", outProfile.learnedEmptyMv);
    outProfile.learnedFullMv = prefs_.getUShort("bat_full", outProfile.learnedFullMv);
    outProfile.curveBias = prefs_.getShort("bat_bias", outProfile.curveBias);
    outProfile.avgDischargeRate = prefs_.getFloat("bat_rate", outProfile.avgDischargeRate);
    outProfile.sampleCount = prefs_.getUShort("bat_samples", outProfile.sampleCount);
    outProfile.initialized = prefs_.getBool("bat_init", outProfile.initialized);

    if (!isValidBatteryProfile(outProfile)) {
        outProfile = BatteryProfile{};
        return false;
    }

    return true;
}

bool ConfigStore::saveBatteryProfile(const BatteryProfile& profile) {
    bool ok = true;
    ok = ok && prefs_.putUShort("bat_empty", profile.learnedEmptyMv) > 0;
    ok = ok && prefs_.putUShort("bat_full", profile.learnedFullMv) > 0;
    ok = ok && prefs_.putShort("bat_bias", profile.curveBias) > 0;
    ok = ok && prefs_.putFloat("bat_rate", profile.avgDischargeRate) > 0;
    ok = ok && prefs_.putUShort("bat_samples", profile.sampleCount) > 0;
    ok = ok && prefs_.putBool("bat_init", profile.initialized) > 0;
    return ok;
}
