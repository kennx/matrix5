#pragma once

#include <stdint.h>
#include <string>

enum class ConfigError : uint16_t {
    Ok = 0,
    JsonParseFailed = 2001,
    InvalidField = 2002,
    WifiConnectFailed = 3001,
    NtpSyncFailed = 3002,
    WifiScanFailed = 3003,
};

enum class ConfigState : uint8_t {
    Idle,
    ConfigReceived,
    Applying,
    Done,
    Error,
    ScanComplete = 9,
};

struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPassword;
    std::string timezone;
    std::string ntpServer;
    uint8_t brightness = 50;
};
