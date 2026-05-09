#pragma once

#include "config_types.h"

class PairingCodeManager {
   public:
    std::string generate(uint32_t nowMs, uint32_t ttlMs);
    ConfigError verify(const std::string& inputCode, uint32_t nowMs);

   private:
    std::string activeCode_;
    uint32_t expireAtMs_ = 0;
};
