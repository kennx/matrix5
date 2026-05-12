#include "time_sync_utils.h"
#include "../../../src/timezone_convert.cpp"
#include "../../../src/time_sync_utils.cpp"

#include <assert.h>
#include <time.h>

int main() {
    time_t epoch = 0;
    struct tm tmValue;

    applyTimezoneEnv("Asia/Shanghai");
    localtime_r(&epoch, &tmValue);
    assert(tmValue.tm_hour == 8);

    applyTimezoneEnv("UTC");
    localtime_r(&epoch, &tmValue);
    assert(tmValue.tm_hour == 0);

    return 0;
}
