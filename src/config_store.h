#pragma once

#include "config_types.h"

#include <Preferences.h>

class ConfigStore {
   public:
    explicit ConfigStore(Preferences& prefs) : prefs_(prefs) {}

    bool load(DeviceConfig& outConfig);
    bool save(const DeviceConfig& config);

   private:
    Preferences& prefs_;
};
