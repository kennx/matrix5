#include "pomodoro.h"
#include <cstdio>

const PomodoroModeConfig Pomodoro::MODES[MODE_COUNT] = {
    {"NORMAL", 25,  5, 30, 4},
    {"WORK",   50, 10, 30, 2},
    {"STUDY",  45, 15, 40, 3},
    {"MINI",   15,  2, 10, 4},
};

const PomodoroModeConfig& Pomodoro::currentMode() const {
    return MODES[modeIndex_];
}

bool Pomodoro::isActive() const {
    return state_ != State::Inactive;
}

Pomodoro::State Pomodoro::getState() const {
    return state_;
}

Pomodoro::Phase Pomodoro::getPhase() const {
    return phase_;
}

uint8_t Pomodoro::getModeIndex() const {
    return modeIndex_;
}

const char* Pomodoro::getModeDisplayName() const {
    return MODES[modeIndex_].name;
}

void Pomodoro::getModeWorkTimeDisplay(char* buf, size_t len) const {
    snprintf(buf, len, "%d MIN", MODES[modeIndex_].workMin);
}

void Pomodoro::getTimeDisplay(char* buf, size_t len) const {
    int mins = remainingSeconds_ / 60;
    int secs = remainingSeconds_ % 60;
    snprintf(buf, len, "%02d:%02d", mins, secs);
}

bool Pomodoro::shouldBeep() const {
    return beepFlag_;
}

void Pomodoro::clearBeep() {
    beepFlag_ = false;
}

void Pomodoro::enter() {
    state_ = State::ModeSelect;
    modeIndex_ = 0;
}

void Pomodoro::exit() {
    state_ = State::Inactive;
    beepFlag_ = false;
}

void Pomodoro::startPhase(Phase phase) {
    phase_ = phase;
    const auto& mode = currentMode();
    switch (phase) {
        case Phase::Work:       remainingSeconds_ = mode.workMin * 60;       break;
        case Phase::ShortBreak: remainingSeconds_ = mode.shortBreakMin * 60; break;
        case Phase::LongBreak:  remainingSeconds_ = mode.longBreakMin * 60;  break;
    }
}

void Pomodoro::advanceToNextPhase() {
    const auto& mode = currentMode();
    switch (phase_) {
        case Phase::Work:
            if (currentRound_ >= mode.roundsBeforeLong) {
                startPhase(Phase::LongBreak);
            } else {
                startPhase(Phase::ShortBreak);
            }
            break;
        case Phase::ShortBreak:
            currentRound_++;
            startPhase(Phase::Work);
            break;
        case Phase::LongBreak:
            currentRound_ = 1;
            startPhase(Phase::Work);
            break;
    }
}

void Pomodoro::onBtnAClick() {
    if (state_ == State::ModeSelect) {
        modeIndex_ = (modeIndex_ + 1) % MODE_COUNT;
    }
}

void Pomodoro::onBtnALongPress() {
    if (state_ == State::ModeSelect) {
        currentRound_ = 1;
        startPhase(Phase::Work);
        state_ = State::Running;
        tickSeeded_ = false;
    } else if (state_ == State::Running || state_ == State::Paused) {
        advanceToNextPhase();
        state_ = State::Running;
        tickSeeded_ = false;
    }
}

void Pomodoro::onBtnBClick() {
    if (state_ == State::Running) {
        state_ = State::Paused;
    } else if (state_ == State::Paused) {
        state_ = State::Running;
        tickSeeded_ = false;
    }
}

void Pomodoro::onBtnBLongPress() {
    if (state_ == State::ModeSelect) {
        exit();
    } else if (state_ == State::Running || state_ == State::Paused) {
        state_ = State::ModeSelect;
    }
}

void Pomodoro::update(unsigned long nowMs) {
    if (state_ != State::Running) return;

    if (!tickSeeded_) {
        tickSeeded_ = true;
        lastTickMs_ = nowMs;
        return;
    }

    unsigned long elapsed = nowMs - lastTickMs_;
    if (elapsed >= 1000) {
        int ticks = static_cast<int>(elapsed / 1000);
        lastTickMs_ += ticks * 1000UL;
        remainingSeconds_ -= ticks;

        if (remainingSeconds_ <= 0) {
            remainingSeconds_ = 0;
            beepFlag_ = true;
            advanceToNextPhase();
        }
    }
}
