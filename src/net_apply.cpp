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
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        return ConfigError::WifiConnectFailed;
    }

    std::string posixTz = resolvePosixTimezone(config.timezone);
    applyTimezoneEnv(config.timezone);
    configTzTime(posixTz.c_str(), config.ntpServer.c_str(), "pool.ntp.org", "time.google.com");

    struct tm t;
    bool syncSuccess = getLocalTime(&t, 5000);

    // 断开 WiFi 连接并关闭 WiFi 模块，大幅度降低待机功耗 (节约 ~100mA)
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);

    if (!syncSuccess) {
        return ConfigError::NtpSyncFailed;
    }

    return ConfigError::Ok;
}
