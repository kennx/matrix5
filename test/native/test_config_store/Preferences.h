#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>

class Preferences {
   public:
    std::string getString(const char* key, const char* defaultValue = "") const {
        auto it = strings_.find(key);
        if (it != strings_.end()) {
            return it->second;
        }
        return defaultValue;
    }

    uint8_t getUChar(const char* key, uint8_t defaultValue = 0) const {
        auto it = uchars_.find(key);
        if (it != uchars_.end()) {
            return it->second;
        }
        return defaultValue;
    }

    uint16_t getUShort(const char* key, uint16_t defaultValue = 0) const {
        auto it = ushorts_.find(key);
        if (it != ushorts_.end()) {
            return it->second;
        }
        return defaultValue;
    }

    int16_t getShort(const char* key, int16_t defaultValue = 0) const {
        auto it = shorts_.find(key);
        if (it != shorts_.end()) {
            return it->second;
        }
        return defaultValue;
    }

    float getFloat(const char* key, float defaultValue = 0.0f) const {
        auto it = floats_.find(key);
        if (it != floats_.end()) {
            return it->second;
        }
        return defaultValue;
    }

    bool getBool(const char* key, bool defaultValue = false) const {
        auto it = bools_.find(key);
        if (it != bools_.end()) {
            return it->second;
        }
        return defaultValue;
    }

    size_t putString(const char* key, const char* value) {
        std::string stored = value != nullptr ? value : "";
        strings_[key] = stored;
        return stored.size();
    }

    size_t putUChar(const char* key, uint8_t value) {
        uchars_[key] = value;
        return 1;
    }

    size_t putUShort(const char* key, uint16_t value) {
        ushorts_[key] = value;
        return 2;
    }

    size_t putShort(const char* key, int16_t value) {
        shorts_[key] = value;
        return 2;
    }

    size_t putFloat(const char* key, float value) {
        floats_[key] = value;
        return sizeof(float);
    }

    size_t putBool(const char* key, bool value) {
        bools_[key] = value;
        return 1;
    }

    bool isKey(const char* key) const {
        return strings_.count(key) > 0 || uchars_.count(key) > 0 || ushorts_.count(key) > 0 ||
               shorts_.count(key) > 0 || floats_.count(key) > 0 || bools_.count(key) > 0;
    }

   private:
    std::unordered_map<std::string, std::string> strings_;
    std::unordered_map<std::string, uint8_t> uchars_;
    std::unordered_map<std::string, uint16_t> ushorts_;
    std::unordered_map<std::string, int16_t> shorts_;
    std::unordered_map<std::string, float> floats_;
    std::unordered_map<std::string, bool> bools_;
};
