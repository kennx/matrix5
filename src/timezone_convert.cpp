#include "timezone_convert.h"

std::string resolvePosixTimezone(const std::string& timezoneInput) {
    if (timezoneInput == "Asia/Shanghai") {
        return "CST-8";
    }
    if (timezoneInput == "UTC") {
        return "UTC0";
    }
    if (timezoneInput == "Asia/Tokyo") {
        return "JST-9";
    }
    return timezoneInput;
}
