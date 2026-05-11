#include "pairing_code.h"

#include <stdio.h>

std::string PairingCodeManager::generate(uint32_t nowMs, uint32_t ttlMs) {
    uint32_t raw = nowMs % 1000000;
    char buf[7];
    snprintf(buf, sizeof(buf), "%06u", static_cast<unsigned>(raw));
    activeCode_ = buf;
    expireAtMs_ = nowMs + ttlMs;
    return activeCode_;
}

ConfigError PairingCodeManager::verify(const std::string& inputCode, uint32_t nowMs) {
    if (activeCode_.empty() || inputCode != activeCode_) {
        return ConfigError::InvalidField;
    }
    if (nowMs > expireAtMs_) {
        activeCode_.clear();
        expireAtMs_ = 0;
        return ConfigError::InvalidField;
    }
    activeCode_.clear();
    expireAtMs_ = 0;
    return ConfigError::Ok;
}
