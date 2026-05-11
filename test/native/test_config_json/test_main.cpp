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
    assert(cfg.brightness == 50);

    DeviceConfig noWifi;
    const bool ok2 = parseConfigJson(
        "{\"timezone\":\"Asia/Shanghai\",\"ntpServer\":\"ntp.aliyun.com\"}",
        noWifi);
    assert(ok2);
    assert(noWifi.wifiSsid.empty());
    assert(noWifi.wifiPassword.empty());

    DeviceConfig withBright;
    const bool ok3 = parseConfigJson(
        "{\"timezone\":\"Asia/Shanghai\",\"ntpServer\":\"ntp.aliyun.com\",\"brightness\":75}",
        withBright);
    assert(ok3);
    assert(withBright.brightness == 75);

    DeviceConfig bad;
    assert(!parseConfigJson("{\"wifiSsid\":\"Home\"}", bad));
    return 0;
}
