#pragma once

#include "battery_estimator.h"
#include "config_types.h"

#include <Preferences.h>

class ConfigStore {
   public:
    explicit ConfigStore(Preferences& prefs) : prefs_(prefs) {}

    bool load(DeviceConfig& outConfig);
    bool save(const DeviceConfig& config);
    bool loadBatteryProfile(BatteryProfile& outProfile);
    bool saveBatteryProfile(const BatteryProfile& profile);

   private:
    Preferences& prefs_;
};
