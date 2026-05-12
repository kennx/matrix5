#include "usb_config_service.h"

#include <Arduino.h>

namespace {
constexpr size_t kMaxRxLineLength = 512;
}

void UsbConfigService::begin(const UsbConfigCallbacks& callbacks) {
    callbacks_ = callbacks;
}

void UsbConfigService::loop() {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());

        if (discardUntilNewline_) {
            if (c == '\n') {
                discardUntilNewline_ = false;
            }
            continue;
        }

        if (c == '\n') {
            processLine(rxBuffer_);
            rxBuffer_.clear();
        } else if (c != '\r') {
            if (rxBuffer_.size() >= kMaxRxLineLength) {
                rxBuffer_.clear();
                discardUntilNewline_ = true;
                sendError(ConfigError::JsonParseFailed, "line_too_long");
            } else {
                rxBuffer_ += c;
            }
        }
    }
}

void UsbConfigService::processLine(const std::string& line) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        sendError(ConfigError::JsonParseFailed, err.c_str());
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) {
        sendError(ConfigError::JsonParseFailed, "missing cmd field");
        return;
    }

    if (std::strcmp(cmd, "apply") == 0) {
        JsonObject config = doc["config"];
        if (config.isNull()) {
            sendError(ConfigError::InvalidField, "missing config object");
            return;
        }

        DeviceConfig cfg;
        cfg.wifiSsid = config["wifiSsid"] | "";
        cfg.wifiPassword = config["wifiPassword"] | "";
        cfg.timezone = config["timezone"] | "";
        cfg.ntpServer = config["ntpServer"] | "";
        cfg.brightness = config["brightness"] | 50;

        pendingConfig_ = cfg;
        pendingTime_ = doc["time"] | 0;
        hasPendingApply_ = true;

        if (callbacks_.onApply) {
            callbacks_.onApply(pendingConfig_);
        }
    } else if (std::strcmp(cmd, "scan_wifi") == 0) {
        hasScanRequest_ = true;
        if (callbacks_.onScanWifi) {
            callbacks_.onScanWifi();
        }
    } else if (std::strcmp(cmd, "get_config") == 0) {
        if (callbacks_.onGetConfig) {
            callbacks_.onGetConfig();
        }
    } else {
        sendError(ConfigError::InvalidField, "unknown cmd");
    }
}

void UsbConfigService::sendConfig(const DeviceConfig* cfg) {
    StaticJsonDocument<512> doc;
    doc["type"] = "config";
    if (cfg) {
        JsonObject data = doc.createNestedObject("data");
        data["wifiSsid"] = cfg->wifiSsid;
        data["wifiPassword"] = cfg->wifiPassword;
        data["timezone"] = cfg->timezone;
        data["ntpServer"] = cfg->ntpServer;
        data["brightness"] = cfg->brightness;
    } else {
        doc["data"] = static_cast<const char*>(nullptr);
    }
    serializeJson(doc, Serial);
    Serial.println();
}

void UsbConfigService::sendScanResult(const std::string& json) {
    Serial.println(json.c_str());
}

void UsbConfigService::sendOk(const char* message) {
    StaticJsonDocument<256> doc;
    doc["type"] = "ok";
    doc["message"] = message;
    serializeJson(doc, Serial);
    Serial.println();
}

void UsbConfigService::sendError(ConfigError code, const char* message) {
    StaticJsonDocument<256> doc;
    doc["type"] = "error";
    doc["code"] = static_cast<uint16_t>(code);
    doc["message"] = message;
    serializeJson(doc, Serial);
    Serial.println();
}

bool UsbConfigService::hasPendingApply() const {
    return hasPendingApply_;
}

DeviceConfig UsbConfigService::getPendingConfig() const {
    return pendingConfig_;
}

uint32_t UsbConfigService::getPendingTime() const {
    return pendingTime_;
}

bool UsbConfigService::hasScanRequest() const {
    return hasScanRequest_;
}

void UsbConfigService::clearPendingApply() {
    hasPendingApply_ = false;
}

void UsbConfigService::clearScanRequest() {
    hasScanRequest_ = false;
}
