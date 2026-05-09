#pragma once

#include "config_types.h"

#include <string>

bool parseConfigJson(const std::string& json, DeviceConfig& outConfig);
