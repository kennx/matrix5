#include "config_json.h"
#include "../../../src/config_json.cpp"

#include <assert.h>

int main() {
    DeviceConfig cfg;
    const bool ok = parseConfigJson(
        "{\"wifiSsid\":\"Home\",\"wifiPassword\":\"12345678\",\"timezone\":\"Asia/Shanghai\",\"ntpServer\":\"ntp.aliyun.com\"}",
        cfg);
    assert(ok);
    assert(cfg.wifiSsid == "Home");
    assert(cfg.wifiPassword == "12345678");
    assert(cfg.timezone == "Asia/Shanghai");
    assert(cfg.ntpServer == "ntp.aliyun.com");

    DeviceConfig bad;
    assert(!parseConfigJson("{\"wifiSsid\":\"Home\"}", bad));
    return 0;
}
