#include "net_apply.h"

#include <WiFi.h>
#include <time.h>

ConfigError applyNetworkConfig(const DeviceConfig& config) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        return ConfigError::WifiConnectFailed;
    }

    setenv("TZ", config.timezone.c_str(), 1);
    tzset();
    configTime(0, 0, config.ntpServer.c_str());

    struct tm t;
    if (!getLocalTime(&t, 5000)) {
        return ConfigError::NtpSyncFailed;
    }

    return ConfigError::Ok;
}
