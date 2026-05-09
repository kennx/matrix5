#include "timezone_convert.h"
#include "../../../src/timezone_convert.cpp"

#include <assert.h>

int main() {
    assert(resolvePosixTimezone("Asia/Shanghai") == "CST-8");
    assert(resolvePosixTimezone("UTC") == "UTC0");
    assert(resolvePosixTimezone("CST-8") == "CST-8");
    return 0;
}
