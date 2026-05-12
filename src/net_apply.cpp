#include "net_apply.h"

#include "time_sync_utils.h"
#include "timezone_convert.h"

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

    std::string posixTz = resolvePosixTimezone(config.timezone);
    applyTimezoneEnv(config.timezone);
    configTzTime(posixTz.c_str(), config.ntpServer.c_str(), "pool.ntp.org", "time.google.com");

    struct tm t;
    if (!getLocalTime(&t, 5000)) {
        return ConfigError::NtpSyncFailed;
    }

    return ConfigError::Ok;
}
