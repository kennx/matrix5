#include "pairing_code.h"
#include "../../../src/pairing_code.cpp"

#include <assert.h>

int main() {
    PairingCodeManager mgr;

    const uint32_t nowMs = 1000;
    const std::string code = mgr.generate(nowMs, 120000);
    assert(code.size() == 6);
    assert(mgr.verify(code, nowMs + 1000) == ConfigError::Ok);

    assert(mgr.verify(code, nowMs + 2000) == ConfigError::InvalidField);

    const std::string code2 = mgr.generate(nowMs, 120000);
    assert(mgr.verify(code2, nowMs + 120001) == ConfigError::InvalidField);

    return 0;
}
