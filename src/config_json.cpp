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

static bool extractIntField(const std::string& json, const char* key, uint8_t& out) {
    std::string pattern = std::string("\"") + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return false;
    }

    size_t numStart = json.find_first_of("0123456789", colonPos + 1);
    if (numStart == std::string::npos) {
        return false;
    }

    size_t numEnd = json.find_first_not_of("0123456789", numStart);
    if (numEnd == std::string::npos) {
        numEnd = json.size();
    }

    out = static_cast<uint8_t>(std::stoul(json.substr(numStart, numEnd - numStart)));
    return true;
}

bool parseConfigJson(const std::string& json, DeviceConfig& outConfig) {
    extractStringField(json, "wifiSsid", outConfig.wifiSsid);
    extractStringField(json, "wifiPassword", outConfig.wifiPassword);
    if (!extractStringField(json, "timezone", outConfig.timezone)) {
        return false;
    }
    if (!extractStringField(json, "ntpServer", outConfig.ntpServer)) {
        return false;
    }
    uint8_t brightness;
    if (extractIntField(json, "brightness", brightness)) {
        outConfig.brightness = brightness;
    }
    return true;
}
