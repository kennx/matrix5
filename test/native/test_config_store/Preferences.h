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

    size_t putString(const char* key, const char* value) {
        std::string stored = value != nullptr ? value : "";
        strings_[key] = stored;
        return stored.size();
    }

    size_t putUChar(const char* key, uint8_t value) {
        uchars_[key] = value;
        return 1;
    }

    bool isKey(const char* key) const {
        return strings_.count(key) > 0 || uchars_.count(key) > 0;
    }

   private:
    std::unordered_map<std::string, std::string> strings_;
    std::unordered_map<std::string, uint8_t> uchars_;
};
