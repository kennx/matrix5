#include "time_sync_utils.h"

#include "timezone_convert.h"

#include <stdlib.h>
#include <time.h>

void applyTimezoneEnv(const std::string& timezoneInput) {
    const std::string posixTz = resolvePosixTimezone(timezoneInput);
    setenv("TZ", posixTz.c_str(), 1);
    tzset();
}
