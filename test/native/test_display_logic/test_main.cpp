#include "display_logic.h"
#include "../../../src/display_logic.cpp"

#include <cassert>

static void test_force_redraw_ignores_interval() {
    assert(shouldRenderPeriodicFrame(100, 0, 1000, true));
}

static void test_interval_redraw_after_elapsed() {
    assert(shouldRenderPeriodicFrame(1000, 0, 1000, false));
}

static void test_no_redraw_before_interval_without_force() {
    assert(!shouldRenderPeriodicFrame(999, 0, 1000, false));
}

static void test_short_text_does_not_split() {
    assert(choosePortraitTextSplit("WORK", 27) == 0);
    assert(choosePortraitTextSplit("MINI", 27) == 0);
}

static void test_long_text_splits_for_portrait() {
    assert(choosePortraitTextSplit("STUDY", 27) == 3);
    assert(choosePortraitTextSplit("NORMAL", 27) == 3);
}

int main() {
    test_force_redraw_ignores_interval();
    test_interval_redraw_after_elapsed();
    test_no_redraw_before_interval_without_force();
    test_short_text_does_not_split();
    test_long_text_splits_for_portrait();
    return 0;
}
