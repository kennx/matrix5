#include "config_json.h"

static bool extractStringField(const std::string& json, const char* key, std::string& out) {
    std::string pattern = std::string("\"") + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t firstQuote = json.find('"', colonPos + 1);
    if (firstQuote == std::string::npos) {
        return false;
    }

    size_t secondQuote = json.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) {
        return false;
    }

    out = json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    return true;
}

bool parseConfigJson(const std::string& json, DeviceConfig& outConfig) {
    return extractStringField(json, "wifiSsid", outConfig.wifiSsid) &&
           extractStringField(json, "wifiPassword", outConfig.wifiPassword) &&
           extractStringField(json, "timezone", outConfig.timezone) &&
           extractStringField(json, "ntpServer", outConfig.ntpServer);
}
