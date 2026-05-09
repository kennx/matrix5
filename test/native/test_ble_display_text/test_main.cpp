#include "ble_display_text.h"
#include "../../../src/ble_display_text.cpp"

#include <assert.h>

int main() {
    assert(formatPairingCodeLine("123456") == "CODE 123456");
    assert(formatPairingCodeLine("") == "CODE ------");
    return 0;
}
