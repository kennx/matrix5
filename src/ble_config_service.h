#pragma once

#include "config_types.h"

#include <functional>

struct BleConfigCallbacks {
    std::function<ConfigError(const std::string&)> onAuth;
    std::function<ConfigError(const std::string&)> onConfigJson;
    std::function<ConfigError(const std::string&)> onCommand;
};

class BleConfigService {
   public:
    void begin(const BleConfigCallbacks& callbacks);
    void notifyStatus(ConfigState state, ConfigError error, const char* message);
    void stop();

   private:
    BleConfigCallbacks callbacks_;
};
