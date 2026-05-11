#include "config_store.h"

bool ConfigStore::load(DeviceConfig& outConfig) {
    outConfig.wifiSsid = prefs_.getString("ssid", "").c_str();
    outConfig.wifiPassword = prefs_.getString("password", "").c_str();
    outConfig.timezone = prefs_.getString("timezone", "Asia/Shanghai").c_str();
    outConfig.ntpServer = prefs_.getString("ntp_server", "pool.ntp.org").c_str();
    outConfig.brightness = prefs_.getUChar("brightness", 50);
    return !outConfig.timezone.empty() && !outConfig.ntpServer.empty();
}

bool ConfigStore::save(const DeviceConfig& config) {
    bool ok = true;
    ok = ok && prefs_.putString("ssid", config.wifiSsid.c_str()) >= 0;
    ok = ok && prefs_.putString("password", config.wifiPassword.c_str()) >= 0;
    ok = ok && prefs_.putString("timezone", config.timezone.c_str()) > 0;
    ok = ok && prefs_.putString("ntp_server", config.ntpServer.c_str()) > 0;
    ok = ok && prefs_.putUChar("brightness", config.brightness) > 0;
    return ok;
}
