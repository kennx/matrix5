#include "config_validator.h"

static bool isLenInRange(const std::string& s, size_t minLen, size_t maxLen) {
    return s.size() >= minLen && s.size() <= maxLen;
}

ConfigError validateConfig(const DeviceConfig& config) {
    if (!config.wifiSsid.empty() && !isLenInRange(config.wifiSsid, 1, 32)) {
        return ConfigError::InvalidField;
    }
    if (!config.wifiPassword.empty() && config.wifiPassword.size() > 64) {
        return ConfigError::InvalidField;
    }
    if (!isLenInRange(config.timezone, 1, 64)) {
        return ConfigError::InvalidField;
    }
    if (!config.wifiSsid.empty() && !isLenInRange(config.ntpServer, 1, 128)) {
        return ConfigError::InvalidField;
    }
    if (config.wifiSsid.empty() && !config.ntpServer.empty() && config.ntpServer.size() > 128) {
        return ConfigError::InvalidField;
    }
    if (config.brightness < 1 || config.brightness > 100) {
        return ConfigError::InvalidField;
    }
    return ConfigError::Ok;
}
