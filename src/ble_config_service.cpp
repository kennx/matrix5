#include "ble_config_service.h"

#include <NimBLEDevice.h>

namespace {
constexpr char kServiceUuid[] = "91a50001-66d8-4d35-a42b-df7f00c10000";
constexpr char kAuthUuid[] = "91a50002-66d8-4d35-a42b-df7f00c10000";
constexpr char kConfigUuid[] = "91a50003-66d8-4d35-a42b-df7f00c10000";
constexpr char kCommandUuid[] = "91a50004-66d8-4d35-a42b-df7f00c10000";
constexpr char kStatusUuid[] = "91a50005-66d8-4d35-a42b-df7f00c10000";

NimBLECharacteristic* gStatusChar = nullptr;
BleConfigCallbacks gCallbacks;

class AuthCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        if (!gCallbacks.onAuth) {
            return;
        }
        std::string payload = c->getValue();
        ConfigError err = gCallbacks.onAuth(payload);
        if (gStatusChar) {
            std::string result = "{\"state\":" + std::to_string(static_cast<int>(ConfigState::AuthPending)) +
                                 ",\"error\":" + std::to_string(static_cast<int>(err)) +
                                 ",\"message\":\"auth\"}";
            gStatusChar->setValue(result);
            gStatusChar->notify();
        }
    }
};

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        if (!gCallbacks.onConfigJson) {
            return;
        }
        std::string payload = c->getValue();
        ConfigError err = gCallbacks.onConfigJson(payload);
        if (gStatusChar) {
            std::string result = "{\"state\":" + std::to_string(static_cast<int>(ConfigState::ConfigReceived)) +
                                 ",\"error\":" + std::to_string(static_cast<int>(err)) +
                                 ",\"message\":\"config\"}";
            gStatusChar->setValue(result);
            gStatusChar->notify();
        }
    }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        if (!gCallbacks.onCommand) {
            return;
        }
        std::string payload = c->getValue();
        ConfigError err = gCallbacks.onCommand(payload);
        if (gStatusChar) {
            std::string result = "{\"state\":" + std::to_string(static_cast<int>(ConfigState::Applying)) +
                                 ",\"error\":" + std::to_string(static_cast<int>(err)) +
                                 ",\"message\":\"apply\"}";
            gStatusChar->setValue(result);
            gStatusChar->notify();
        }
    }
};
}  // namespace

void BleConfigService::begin(const BleConfigCallbacks& callbacks) {
    callbacks_ = callbacks;
    gCallbacks = callbacks;
    NimBLEDevice::init("matrix5-config");
    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service = server->createService(kServiceUuid);

    NimBLECharacteristic* authChar = service->createCharacteristic(kAuthUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic* configChar = service->createCharacteristic(kConfigUuid, NIMBLE_PROPERTY::WRITE);
    NimBLECharacteristic* commandChar = service->createCharacteristic(kCommandUuid, NIMBLE_PROPERTY::WRITE);
    gStatusChar = service->createCharacteristic(kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    authChar->setCallbacks(new AuthCallbacks());
    configChar->setCallbacks(new ConfigCallbacks());
    commandChar->setCallbacks(new CommandCallbacks());

    service->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(kServiceUuid);
    adv->start();
}

void BleConfigService::publishResult(ConfigState state, ConfigError error, const char* message) {
    notifyStatus(state, error, message);
}

void BleConfigService::notifyStatus(ConfigState state, ConfigError error, const char* message) {
    if (!gStatusChar) {
        return;
    }
    std::string payload = "{\"state\":" + std::to_string(static_cast<int>(state)) +
                          ",\"error\":" + std::to_string(static_cast<int>(error)) +
                          ",\"message\":\"" + message + "\"}";
    gStatusChar->setValue(payload);
    gStatusChar->notify();
}

void BleConfigService::stop() {
    NimBLEDevice::deinit(true);
}
