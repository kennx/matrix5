#include "ble_config_service.h"

#include <NimBLEDevice.h>

namespace {
constexpr char kServiceUuid[] = "91a50001-66d8-4d35-a42b-df7f00c10000";
constexpr char kAuthUuid[] = "91a50002-66d8-4d35-a42b-df7f00c10000";
constexpr char kConfigUuid[] = "91a50003-66d8-4d35-a42b-df7f00c10000";
constexpr char kCommandUuid[] = "91a50004-66d8-4d35-a42b-df7f00c10000";
constexpr char kStatusUuid[] = "91a50005-66d8-4d35-a42b-df7f00c10000";

NimBLECharacteristic* gStatusChar = nullptr;
}  // namespace

void BleConfigService::begin(const BleConfigCallbacks& callbacks) {
    callbacks_ = callbacks;
    NimBLEDevice::init("matrix5-config");
    NimBLEServer* server = NimBLEDevice::createServer();
    NimBLEService* service = server->createService(kServiceUuid);

    service->createCharacteristic(kAuthUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    service->createCharacteristic(kConfigUuid, NIMBLE_PROPERTY::WRITE);
    service->createCharacteristic(kCommandUuid, NIMBLE_PROPERTY::WRITE);
    gStatusChar = service->createCharacteristic(kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    service->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(kServiceUuid);
    adv->start();
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
