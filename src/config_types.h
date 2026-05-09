#pragma once

#include <stdint.h>
#include <string>

enum class ConfigError : uint16_t {
    Ok = 0,
    Unauthorized = 1001,
    PairingCodeInvalid = 1002,
    PairingCodeExpired = 1003,
    JsonParseFailed = 2001,
    InvalidField = 2002,
    WifiConnectFailed = 3001,
    NtpSyncFailed = 3002,
};

enum class ConfigState : uint8_t {
    Idle,
    BleAdvertising,
    AuthPending,
    AuthOk,
    ConfigReceived,
    Applying,
    Done,
    Error,
};

struct DeviceConfig {
    std::string wifiSsid;
    std::string wifiPassword;
    std::string timezone;
    std::string ntpServer;
};
