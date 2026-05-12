# 番茄钟实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 M5StickS3 点阵时钟上实现番茄钟功能，支持 4 种模式、倒计时、阶段自动切换、蜂鸣提示。

**Architecture:** 独立 Pomodoro 模块（`pomodoro.h/cpp`）管理状态机和计时逻辑，不依赖硬件 API。main.cpp 负责按键分发、渲染和蜂鸣。

**Tech Stack:** C++ / Arduino / M5Unified / PlatformIO

**Design Spec:** `docs/superpowers/specs/2026-05-12-pomodoro-timer-design.md`

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `src/pomodoro.h` | 新建 | Pomodoro 类声明、枚举、配置结构体 |
| `src/pomodoro.cpp` | 新建 | 状态机实现、计时、阶段转换 |
| `test/native/test_pomodoro/test_main.cpp` | 新建 | Pomodoro 纯逻辑单元测试 |
| `src/main.cpp` | 修改 | 集成 Pomodoro：按键分发、渲染、蜂鸣 |

---

### Task 1: Pomodoro 类声明

**Files:**
- Create: `src/pomodoro.h`

- [ ] **Step 1: 创建 pomodoro.h**

```cpp
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
```

- [ ] **Step 2: 确认编译通过**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio build -e native`
Expected: 编译成功（头文件未被引用，不影响构建）

- [ ] **Step 3: Commit**

```bash
git add src/pomodoro.h
git commit -m "feat(pomodoro): 添加 Pomodoro 类声明

- State 枚举: Inactive/ModeSelect/Running/Paused
- Phase 枚举: Work/ShortBreak/LongBreak
- PomodoroModeConfig 结构体
- 公开接口: enter/exit/按键/update/显示/beep"
```

---

### Task 2: Pomodoro 核心逻辑实现

**Files:**
- Create: `src/pomodoro.cpp`

- [ ] **Step 1: 创建 pomodoro.cpp**

```cpp
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
        lastTickMs_ = 0;  // will be set on first update()
    } else if (state_ == State::Running || state_ == State::Paused) {
        advanceToNextPhase();
        state_ = State::Running;
        lastTickMs_ = 0;
    }
}

void Pomodoro::onBtnBClick() {
    if (state_ == State::Running) {
        state_ = State::Paused;
    } else if (state_ == State::Paused) {
        state_ = State::Running;
        lastTickMs_ = 0;  // reset tick base to avoid time jump
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

    if (lastTickMs_ == 0) {
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
```

- [ ] **Step 2: 确认 native 环境编译通过**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio build -e native`
Expected: 编译成功（pomodoro.cpp 未被 native 引用，不影响构建）

- [ ] **Step 3: Commit**

```bash
git add src/pomodoro.cpp
git commit -m "feat(pomodoro): 实现状态机核心逻辑

- 4 种模式配置: Normal/Work/Study/Mini
- 状态转换: ModeSelect → Running ↔ Paused
- 阶段转换: Work → ShortBreak/LongBreak → Work（循环）
- 倒计时: update() 基于 millis 差值每秒递减
- beep 标志: 阶段自然结束时设置"
```

---

### Task 3: Pomodoro 单元测试

**Files:**
- Create: `test/native/test_pomodoro/test_main.cpp`

- [ ] **Step 1: 创建测试文件**

```cpp
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

    p.update(11000);  // 1 second after resume
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
```

- [ ] **Step 2: 运行测试并确认全部通过**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio test -e native -f test_pomodoro -v`
Expected: 所有 assert 通过，返回 0

- [ ] **Step 3: Commit**

```bash
git add test/native/test_pomodoro/test_main.cpp
git commit -m "test(pomodoro): 添加状态机单元测试

- 覆盖: 初始状态、模式切换、开始计时、倒计时
- 覆盖: 暂停/恢复、阶段转换、长休息触发、循环重置
- 覆盖: 手动跳过阶段、退出、按键忽略"
```

---

### Task 4: main.cpp 集成 — 按键分发与状态切换

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 添加 include 和全局对象**

在 `src/main.cpp` 顶部 include 区域，在 `#include "usb_config_service.h"` 之后添加：

```cpp
#include "pomodoro.h"
```

在全局变量区域，在 `M5Canvas sprite(&M5.Display);` 之后添加：

```cpp
Pomodoro pomodoro;
```

- [ ] **Step 2: 在 loop() 中添加 Pomodoro 分支**

在 `loop()` 中，将现有的 `} else {`（非 configMode 分支，约 383 行开始）改为三路分支。把原来的 else 块内容（384-431 行）替换为：

```cpp
    } else if (pomodoro.isActive()) {
        // --- 番茄钟活跃 ---
        pomodoro.update(millis());

        // 蜂鸣
        if (pomodoro.shouldBeep()) {
            M5.Speaker.tone(1000, 200);
            pomodoro.clearBeep();
        }

        // 按键分发
        if (M5.BtnA.wasClicked()) {
            pomodoro.onBtnAClick();
        }
        static bool pomoBtnALongTriggered = false;
        if (M5.BtnA.pressedFor(3000)) {
            if (!pomoBtnALongTriggered) {
                pomoBtnALongTriggered = true;
                pomodoro.onBtnALongPress();
            }
        } else {
            pomoBtnALongTriggered = false;
        }

        if (M5.BtnB.wasClicked()) {
            pomodoro.onBtnBClick();
        }
        static bool pomoBtnBLongTriggered = false;
        if (M5.BtnB.pressedFor(3000)) {
            if (!pomoBtnBLongTriggered) {
                pomoBtnBLongTriggered = true;
                pomodoro.onBtnBLongPress();
            }
        } else {
            pomoBtnBLongTriggered = false;
        }

        // 渲染（每秒更新）
        static unsigned long lastPomoUpdate = 0;
        if (millis() - lastPomoUpdate >= 1000 ||
            pomodoro.getState() == Pomodoro::State::ModeSelect) {
            lastPomoUpdate = millis();
            if (pomodoro.getState() == Pomodoro::State::ModeSelect) {
                drawBattery(pomodoro.getModeDisplayName());
            } else {
                char buf[6];
                pomodoro.getTimeDisplay(buf, sizeof(buf));
                drawClock(buf);
            }
        }
    } else {
        // --- 原有时钟/日期/电量逻辑 ---
        if (M5.BtnA.wasClicked()) {
            displayMode = static_cast<DisplayMode>(
                (static_cast<int>(displayMode) + 1) % static_cast<int>(DisplayMode::Count));
        }

        // 长按 BtnA 3s 进入番茄钟
        static bool clockBtnALongTriggered = false;
        if (M5.BtnA.pressedFor(3000)) {
            if (!clockBtnALongTriggered) {
                clockBtnALongTriggered = true;
                pomodoro.enter();
            }
        } else {
            clockBtnALongTriggered = false;
        }

        if (millis() - lastUpdate >= 1000) {
            lastUpdate = millis();
            // ... 原有的时钟/日期/电量渲染逻辑保持不变 ...
```

注意：原有的时钟/日期/电量渲染代码（`struct tm timeinfo;` 开始到该 else 块结束）保持不变，只是将其包裹在新的 else 分支中。

- [ ] **Step 3: 确认固件编译通过**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio build -e m5stick-s3`
Expected: 编译成功

- [ ] **Step 4: 确认 native 测试仍通过**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio test -e native -v`
Expected: 所有测试通过

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat(pomodoro): 集成番茄钟到 main loop

- 三路分支: configMode → pomodoro → 时钟
- 长按 BtnA 3s 从时钟进入番茄钟
- 按键分发含防重触发标志位
- ModeSelect 复用 drawBattery 居中渲染
- Running/Paused 复用 drawClock 渲染 MM:SS
- 阶段结束蜂鸣: M5.Speaker.tone(1000, 200)"
```

---

### Task 5: 验证 & 上传固件

- [ ] **Step 1: 运行全部 native 测试**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio test -e native -v`
Expected: 所有测试通过

- [ ] **Step 2: 编译固件**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio build -e m5stick-s3`
Expected: 编译成功，无 warning

- [ ] **Step 3: 上传固件到设备**

Run: `cd /Users/kenn/PROJECTS/m5stack/matrix5 && pio run -e m5stick-s3 -t upload`
Expected: 固件上传成功

- [ ] **Step 4: 手动测试清单**

在设备上逐项验证：
1. 时钟界面 click BtnA → 仍正常切换 Clock/Date/Battery
2. 时钟界面长按 BtnA 3s → 进入番茄钟，显示 "NORMAL"
3. click BtnA → 循环 NORMAL → WORK → STUDY → MINI → NORMAL
4. 长按 BtnA 3s → 开始计时，显示 25:00 倒计时
5. click BtnB → 暂停，倒计时停止
6. click BtnB → 恢复，倒计时继续
7. 长按 BtnA 3s → 跳过到休息阶段
8. 长按 BtnB 3s → 退回模式选择页
9. 模式选择页长按 BtnB 3s → 退回时钟
10. BtnA+BtnB 5s → 仍能进入 USB 配置模式
