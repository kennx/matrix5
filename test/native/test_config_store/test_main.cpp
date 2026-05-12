#include "config_store.h"
#include "config_types.h"
#include "../../../src/config_store.cpp"

#include <assert.h>

int main() {
    Preferences prefs;
    ConfigStore store(prefs);

    DeviceConfig noWifiNoNtp{"", "", "Asia/Shanghai", "", 60};
    assert(store.save(noWifiNoNtp));

    DeviceConfig loaded;
    assert(store.load(loaded));
    assert(loaded.wifiSsid.empty());
    assert(loaded.ntpServer.empty());
    assert(loaded.timezone == "Asia/Shanghai");
    assert(loaded.brightness == 60);

    return 0;
}
