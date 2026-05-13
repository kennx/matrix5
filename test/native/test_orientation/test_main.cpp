#include "orientation.h"
#include "../../../src/orientation.cpp"

#include <cassert>

static void test_initial_state() {
    OrientationManager mgr;
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
    assert(!mgr.orientationJustChanged());
}

static void test_landscape_from_accel() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
}

static void test_portrait_from_accel() {
    OrientationManager mgr;
    mgr.update(0.0f, 1.0f, 0);   // first call sets pending
    mgr.update(0.0f, 1.0f, 500); // second call after debounce changes it
    assert(mgr.getOrientation() == ScreenOrientation::Portrait);
}

static void test_debounce_no_change_on_quick_flip() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);
    mgr.update(0.0f, 1.0f, 100);
    mgr.update(1.0f, 0.0f, 200);
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
}

static void test_debounce_changes_after_500ms() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);
    mgr.update(0.0f, 1.0f, 0);
    mgr.update(0.0f, 1.0f, 500);
    assert(mgr.getOrientation() == ScreenOrientation::Portrait);
    assert(mgr.orientationJustChanged());
}

static void test_just_changed_clears_on_next_update() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);
    mgr.update(0.0f, 1.0f, 0);
    mgr.update(0.0f, 1.0f, 500);
    assert(mgr.orientationJustChanged());

    mgr.update(0.0f, 1.0f, 501);
    assert(!mgr.orientationJustChanged());
}

static void test_diagonal_defaults_to_landscape() {
    OrientationManager mgr;
    mgr.update(0.7f, 0.7f, 0);
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);
}

int main() {
    test_initial_state();
    test_landscape_from_accel();
    test_portrait_from_accel();
    test_debounce_no_change_on_quick_flip();
    test_debounce_changes_after_500ms();
    test_just_changed_clears_on_next_update();
    test_diagonal_defaults_to_landscape();
    return 0;
}
