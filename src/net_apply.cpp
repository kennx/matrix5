#include "net_apply.h"

#include "time_sync_utils.h"
#include "timezone_convert.h"

#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>

ConfigError applyNetworkConfig(const DeviceConfig& config) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
        return ConfigError::WifiConnectFailed;
    }

    std::string posixTz = resolvePosixTimezone(config.timezone);
    applyTimezoneEnv(config.timezone);
    
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTzTime(posixTz.c_str(), config.ntpServer.c_str(), "pool.ntp.org", "time.google.com");

    bool syncSuccess = false;
    for (int i = 0; i < 20; i++) {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            syncSuccess = true;
            break;
        }
        delay(500);
    }

    // 断开 WiFi 连接并关闭 WiFi 模块，大幅度降低待机功耗 (节约 ~100mA)
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);

    if (!syncSuccess) {
        return ConfigError::NtpSyncFailed;
    }

    return ConfigError::Ok;
}
