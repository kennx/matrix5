#include "config_types.h"
#include "config_validator.h"
#include "../../../src/config_validator.cpp"

#include <assert.h>

int main() {
    DeviceConfig ok{"HomeWiFi", "12345678", "Asia/Shanghai", "ntp.aliyun.com", 50};
    assert(validateConfig(ok) == ConfigError::Ok);

    DeviceConfig noWifi{"", "", "Asia/Shanghai", "ntp.aliyun.com", 50};
    assert(validateConfig(noWifi) == ConfigError::Ok);

    DeviceConfig badSsid{"ThisSSIDIsWayTooLongAndExceedsThirtyTwo", "12345678", "Asia/Shanghai", "ntp.aliyun.com", 50};
    assert(validateConfig(badSsid) == ConfigError::InvalidField);

    DeviceConfig badTz{"HomeWiFi", "12345678", "", "ntp.aliyun.com", 50};
    assert(validateConfig(badTz) == ConfigError::InvalidField);

    DeviceConfig badNtp{"HomeWiFi", "12345678", "Asia/Shanghai", "", 50};
    assert(validateConfig(badNtp) == ConfigError::InvalidField);

    DeviceConfig badBright{"HomeWiFi", "12345678", "Asia/Shanghai", "ntp.aliyun.com", 0};
    assert(validateConfig(badBright) == ConfigError::InvalidField);

    DeviceConfig badBright2{"HomeWiFi", "12345678", "Asia/Shanghai", "ntp.aliyun.com", 101};
    assert(validateConfig(badBright2) == ConfigError::InvalidField);

    return 0;
}
