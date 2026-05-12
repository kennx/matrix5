#pragma once

#include <cstdint>
#include <cstddef>

struct PomodoroModeConfig {
    const char* name;
    uint16_t workMin;
    uint16_t shortBreakMin;
    uint16_t longBreakMin;
    uint8_t  roundsBeforeLong;
};

class Pomodoro {
public:
    enum class State : uint8_t { Inactive, ModeSelect, Running, Paused };
    enum class Phase : uint8_t { Work, ShortBreak, LongBreak };

    void enter();
    void exit();

    void onBtnAClick();
    void onBtnALongPress();
    void onBtnBClick();
    void onBtnBLongPress();

    void update(unsigned long nowMs);

    bool isActive() const;
    State getState() const;
    Phase getPhase() const;
    uint8_t getModeIndex() const;
    const char* getModeDisplayName() const;
    void getTimeDisplay(char* buf, size_t len) const;

    bool shouldBeep() const;
    void clearBeep();

    static constexpr uint8_t MODE_COUNT = 4;

private:
    static const PomodoroModeConfig MODES[MODE_COUNT];

    State state_ = State::Inactive;
    Phase phase_ = Phase::Work;
    uint8_t modeIndex_ = 0;
    uint8_t currentRound_ = 1;
    int remainingSeconds_ = 0;
    unsigned long lastTickMs_ = 0;
    bool beepFlag_ = false;

    void startPhase(Phase phase);
    void advanceToNextPhase();
    const PomodoroModeConfig& currentMode() const;
};
