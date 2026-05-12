#include "pomodoro.h"
#include "../../../src/pomodoro.cpp"

#include <cassert>
#include <cstring>

static void test_initial_state() {
    Pomodoro p;
    assert(!p.isActive());
    assert(p.getState() == Pomodoro::State::Inactive);
}

static void test_enter_mode_select() {
    Pomodoro p;
    p.enter();
    assert(p.isActive());
    assert(p.getState() == Pomodoro::State::ModeSelect);
    assert(p.getModeIndex() == 0);
    assert(strcmp(p.getModeDisplayName(), "NORMAL") == 0);
}

static void test_cycle_modes() {
    Pomodoro p;
    p.enter();
    assert(strcmp(p.getModeDisplayName(), "NORMAL") == 0);

    p.onBtnAClick();
    assert(strcmp(p.getModeDisplayName(), "WORK") == 0);

    p.onBtnAClick();
    assert(strcmp(p.getModeDisplayName(), "STUDY") == 0);

    p.onBtnAClick();
    assert(strcmp(p.getModeDisplayName(), "MINI") == 0);

    p.onBtnAClick();  // wraps around
    assert(strcmp(p.getModeDisplayName(), "NORMAL") == 0);
}

static void test_start_timer() {
    Pomodoro p;
    p.enter();
    p.onBtnALongPress();  // start Normal mode

    assert(p.getState() == Pomodoro::State::Running);
    assert(p.getPhase() == Pomodoro::Phase::Work);

    char buf[6];
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "25:00") == 0);
}

static void test_countdown() {
    Pomodoro p;
    p.enter();
    p.onBtnALongPress();

    p.update(0);       // seed lastTickMs_
    p.update(1000);    // 1 second elapsed

    char buf[6];
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "24:59") == 0);

    p.update(4000);    // 3 more seconds
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "24:56") == 0);
}

static void test_pause_resume() {
    Pomodoro p;
    p.enter();
    p.onBtnALongPress();

    p.update(0);
    p.update(2000);  // 2 seconds elapsed

    p.onBtnBClick();  // pause
    assert(p.getState() == Pomodoro::State::Paused);

    p.update(10000);  // 8 more seconds pass while paused
    char buf[6];
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "24:58") == 0);  // still at 24:58, not 24:50

    p.onBtnBClick();  // resume
    assert(p.getState() == Pomodoro::State::Running);

    p.update(11000);  // seed tick base after resume
    p.update(12000);  // 1 second after seed
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "24:57") == 0);
}

static void test_phase_transition_work_to_short_break() {
    // Use Mini mode (15 min work, 2 min break) for shorter test
    Pomodoro p;
    p.enter();
    p.onBtnAClick();  // Work
    p.onBtnAClick();  // Study
    p.onBtnAClick();  // Mini
    p.onBtnALongPress();  // start Mini mode

    assert(p.getPhase() == Pomodoro::Phase::Work);

    // Simulate time passing: 15 minutes = 900 seconds
    p.update(0);
    p.update(900UL * 1000);  // jump to end

    assert(p.shouldBeep());
    assert(p.getPhase() == Pomodoro::Phase::ShortBreak);
    p.clearBeep();
    assert(!p.shouldBeep());

    char buf[6];
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "02:00") == 0);  // Mini short break = 2 min
}

static void test_long_break_after_n_rounds() {
    // Mini mode: 4 rounds before long break
    Pomodoro p;
    p.enter();
    p.onBtnAClick();  // Work
    p.onBtnAClick();  // Study
    p.onBtnAClick();  // Mini
    p.onBtnALongPress();

    unsigned long t = 0;
    p.update(t);

    // Round 1: Work(15min) → ShortBreak(2min)
    t += 900UL * 1000; p.update(t);  // end work
    assert(p.getPhase() == Pomodoro::Phase::ShortBreak);
    p.clearBeep();
    t += 120UL * 1000; p.update(t);  // end short break
    p.clearBeep();

    // Round 2: Work → ShortBreak
    assert(p.getPhase() == Pomodoro::Phase::Work);
    t += 900UL * 1000; p.update(t);
    assert(p.getPhase() == Pomodoro::Phase::ShortBreak);
    p.clearBeep();
    t += 120UL * 1000; p.update(t);
    p.clearBeep();

    // Round 3: Work → ShortBreak
    assert(p.getPhase() == Pomodoro::Phase::Work);
    t += 900UL * 1000; p.update(t);
    assert(p.getPhase() == Pomodoro::Phase::ShortBreak);
    p.clearBeep();
    t += 120UL * 1000; p.update(t);
    p.clearBeep();

    // Round 4: Work → LongBreak
    assert(p.getPhase() == Pomodoro::Phase::Work);
    t += 900UL * 1000; p.update(t);
    assert(p.getPhase() == Pomodoro::Phase::LongBreak);
    p.clearBeep();

    char buf[6];
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "10:00") == 0);  // Mini long break = 10 min

    // After long break → back to round 1
    t += 600UL * 1000; p.update(t);
    assert(p.getPhase() == Pomodoro::Phase::Work);
    p.clearBeep();
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "15:00") == 0);  // back to 15 min work
}

static void test_skip_phase() {
    Pomodoro p;
    p.enter();
    p.onBtnALongPress();  // start Normal

    p.update(0);
    p.update(5000);  // 5 seconds in

    char buf[6];
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "24:55") == 0);

    p.onBtnALongPress();  // skip to next phase
    assert(p.getPhase() == Pomodoro::Phase::ShortBreak);
    assert(!p.shouldBeep());  // no beep on manual skip
    p.getTimeDisplay(buf, sizeof(buf));
    assert(strcmp(buf, "05:00") == 0);
}

static void test_exit_to_mode_select() {
    Pomodoro p;
    p.enter();
    p.onBtnALongPress();  // start
    assert(p.getState() == Pomodoro::State::Running);

    p.onBtnBLongPress();  // exit to mode select
    assert(p.getState() == Pomodoro::State::ModeSelect);
    assert(p.isActive());
}

static void test_exit_from_mode_select() {
    Pomodoro p;
    p.enter();
    assert(p.isActive());

    p.onBtnBLongPress();  // exit to clock
    assert(!p.isActive());
    assert(p.getState() == Pomodoro::State::Inactive);
}

static void test_btnA_click_ignored_during_running() {
    Pomodoro p;
    p.enter();
    p.onBtnALongPress();  // start Normal
    assert(p.getState() == Pomodoro::State::Running);

    p.onBtnAClick();  // should be ignored
    assert(p.getState() == Pomodoro::State::Running);
    assert(p.getPhase() == Pomodoro::Phase::Work);
}

int main() {
    test_initial_state();
    test_enter_mode_select();
    test_cycle_modes();
    test_start_timer();
    test_countdown();
    test_pause_resume();
    test_phase_transition_work_to_short_break();
    test_long_break_after_n_rounds();
    test_skip_phase();
    test_exit_to_mode_select();
    test_exit_from_mode_select();
    test_btnA_click_ignored_during_running();
    return 0;
}
