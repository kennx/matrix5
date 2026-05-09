#include "ble_display_text.h"

std::string formatPairingCodeLine(const std::string& code) {
    if (code.empty()) {
        return "CODE ------";
    }
    return "CODE " + code;
}
