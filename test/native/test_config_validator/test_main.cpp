#include "config_types.h"
#include "config_validator.h"
#include "../../../src/config_validator.cpp"

#include <assert.h>

int main() {
    DeviceConfig ok{"HomeWiFi", "12345678", "Asia/Shanghai", "ntp.aliyun.com"};
    assert(validateConfig(ok) == ConfigError::Ok);

    DeviceConfig badSsid{"", "12345678", "Asia/Shanghai", "ntp.aliyun.com"};
    assert(validateConfig(badSsid) == ConfigError::InvalidField);

    DeviceConfig badTz{"HomeWiFi", "12345678", "", "ntp.aliyun.com"};
    assert(validateConfig(badTz) == ConfigError::InvalidField);

    DeviceConfig badNtp{"HomeWiFi", "12345678", "Asia/Shanghai", ""};
    assert(validateConfig(badNtp) == ConfigError::InvalidField);

    return 0;
}
