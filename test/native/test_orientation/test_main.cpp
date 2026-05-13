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

static void test_wraparound() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0xFFFFFFFF - 300);  // Landscape, near wraparound
    mgr.update(0.0f, 1.0f, 0xFFFFFFFF - 300);  // Portrait, pending starts
    // Should NOT switch yet (0ms elapsed visually, but math works due to unsigned wraparound)
    mgr.update(0.0f, 1.0f, 0xFFFFFFFF - 100);  // 200ms later, still < 500ms
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);

    mgr.update(0.0f, 1.0f, 200);  // wrapped around, total ~500ms elapsed
    assert(mgr.getOrientation() == ScreenOrientation::Portrait);
}

static void test_repeated_direction_changes() {
    OrientationManager mgr;
    mgr.update(1.0f, 0.0f, 0);     // Landscape
    mgr.update(0.0f, 1.0f, 100);   // Portrait at t=100
    mgr.update(1.0f, 0.0f, 200);   // Landscape at t=200
    mgr.update(0.0f, 1.0f, 300);   // Portrait at t=300
    mgr.update(1.0f, 0.0f, 400);   // Landscape at t=400
    assert(mgr.getOrientation() == ScreenOrientation::Landscape);  // never stable enough

    mgr.update(1.0f, 0.0f, 900);   // Landscape stable at t=900
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
    test_wraparound();
    test_repeated_direction_changes();
    return 0;
}
