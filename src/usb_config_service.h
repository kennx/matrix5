#pragma once

#include "config_types.h"

#include <ArduinoJson.h>
#include <cstring>
#include <functional>
#include <string>

struct UsbConfigCallbacks {
    std::function<void(const DeviceConfig&)> onApply;
    std::function<void()> onScanWifi;
    std::function<void()> onGetConfig;
};

class UsbConfigService {
   public:
    void begin(const UsbConfigCallbacks& callbacks);
    void loop();

    void sendConfig(const DeviceConfig* cfg);
    void sendScanResult(const std::string& json);
    void sendOk(const char* message);
    void sendError(ConfigError code, const char* message);

    bool hasPendingApply() const;
    DeviceConfig getPendingConfig() const;
    uint32_t getPendingTime() const;
    bool hasScanRequest() const;
    void clearPendingApply();
    void clearScanRequest();

   private:
    void processLine(const std::string& line);

    UsbConfigCallbacks callbacks_;
    bool hasPendingApply_ = false;
    bool hasScanRequest_ = false;
    DeviceConfig pendingConfig_;
    uint32_t pendingTime_ = 0;
    std::string rxBuffer_;
};
