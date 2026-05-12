# 番茄钟功能设计

## 概述

在 M5StickS3 点阵时钟上增加番茄钟功能。从时钟/日期/电量界面长按 BtnA 3s 进入，支持 4 种预设模式，在 48×27 点阵屏上以 MM:SS 倒计时显示。

## 入口

在时钟/日期/电量界面（非配置模式）下，长按 BtnA 3 秒进入番茄钟模式选择页。

## 模式配置

| 模式 | 显示名 | 工作 | 短休息 | 长休息 | 轮次 |
|------|--------|------|--------|--------|------|
| Normal | NORMAL | 25 min | 5 min | 30 min | 4 轮后 |
| Work | WORK | 50 min | 10 min | 30 min | 2 轮后 |
| Study | STUDY | 45 min | 15 min | 40 min | 3 轮后 |
| Mini | MINI | 15 min | 2 min | 10 min | 4 轮后 |

每个模式的循环：

```
Round 1: Work → ShortBreak
Round 2: Work → ShortBreak
...
Round N: Work → LongBreak → 自动回到 Round 1 继续
```

最后一轮的 Work 结束后进入 LongBreak（替代 ShortBreak），LongBreak 结束后自动回到 Round 1 开始新循环。

## 状态机

三个状态：

- **ModeSelect**：显示模式名称，click BtnA 循环切换
- **Running**：倒计时进行中
- **Paused**：倒计时暂停，画面静止（不闪烁）

```
时钟界面 ──(长按A 3s)──→ ModeSelect ──(长按A 3s)──→ Running ←─(click B)─→ Paused
                              ↑                                                │
                              └─────────────(长按B 3s)──────────────────────────┘
```

## 阶段（Phase）

运行时有三种阶段：

- **Work**：工作阶段
- **ShortBreak**：短休息
- **LongBreak**：长休息

阶段自然结束（倒计时到 0）时自动进入下一阶段。长按 BtnA 3s 可以跳过当前阶段，直接进入下一阶段。

## 按键行为

| 状态 | BtnA click | BtnA 长按 3s | BtnB click | BtnB 长按 3s |
|------|-----------|-------------|-----------|-------------|
| 时钟/日期/电量 | 切换显示模式 | 进入 ModeSelect | 无 | 无 |
| ModeSelect | 循环 N→W→S→M→N | 选中并开始 → Running | 无 | 退出回时钟 |
| Running | 无 | 跳过当前阶段 | 暂停 → Paused | 退出回 ModeSelect |
| Paused | 无 | 跳过当前阶段 | 恢复 → Running | 退出回 ModeSelect |

所有状态下，BtnA+BtnB 同时长按 5s 进入 USB 配置模式（现有行为不变）。

长按事件使用标志位防止 `pressedFor(3000)` 在持续按住时重复触发。

## 架构：方案 A — 独立 Pomodoro 模块（纯逻辑）

### 新增文件

- `src/pomodoro.h` — 类声明
- `src/pomodoro.cpp` — 状态机实现

### Pomodoro 类接口

```cpp
struct PomodoroModeConfig {
    const char* name;
    uint16_t workMin;
    uint16_t shortBreakMin;
    uint16_t longBreakMin;
    uint8_t  roundsBeforeLong;
};

class Pomodoro {
public:
    enum class State { Inactive, ModeSelect, Running, Paused };
    enum class Phase { Work, ShortBreak, LongBreak };

    void enter();                          // 从时钟界面进入 ModeSelect
    void exit();                           // 退回 Inactive

    void onBtnAClick();                    // BtnA 短按
    void onBtnALongPress();                // BtnA 长按 3s
    void onBtnBClick();                    // BtnB 短按
    void onBtnBLongPress();                // BtnB 长按 3s

    void update(unsigned long nowMs);      // 每帧调用，处理倒计时

    bool isActive() const;                 // 是否在番茄钟中（非 Inactive）
    State getState() const;
    Phase getPhase() const;
    const char* getModeDisplayName() const; // 返回当前模式的显示名
    void getTimeDisplay(char* buf, size_t len) const; // 写入 "MM:SS"

    bool shouldBeep() const;               // 是否需要蜂鸣
    void clearBeep();                      // 清除蜂鸣标志

private:
    static const PomodoroModeConfig MODES[4];

    State state_ = State::Inactive;
    Phase phase_ = Phase::Work;
    uint8_t modeIndex_ = 0;               // 0=Normal, 1=Work, 2=Study, 3=Mini
    uint8_t currentRound_ = 1;
    int remainingSeconds_ = 0;
    unsigned long lastTickMs_ = 0;
    bool beepFlag_ = false;

    void startPhase(Phase phase);
    void advanceToNextPhase();
    const PomodoroModeConfig& currentMode() const;
};
```

### 职责分离

- **Pomodoro 模块**：纯逻辑 — 状态管理、计时、阶段转换、提供显示数据
- **main.cpp**：按键分发、渲染（复用现有 drawChar/drawColon）、蜂鸣播放

### main.cpp 集成

```cpp
#include "pomodoro.h"
Pomodoro pomodoro;

void loop() {
    M5.update();

    if (configModeActive) {
        // ... 现有逻辑不变
    } else if (pomodoro.isActive()) {
        pomodoro.update(millis());

        // 按键分发
        if (M5.BtnA.wasClicked()) pomodoro.onBtnAClick();
        // BtnA 长按 3s（带防重触发）
        // BtnB click / 长按同理

        // 蜂鸣
        if (pomodoro.shouldBeep()) {
            M5.Speaker.tone(1000, 200);
            pomodoro.clearBeep();
        }

        // 渲染（每秒更新）
        if (pomodoro.getState() == Pomodoro::State::ModeSelect) {
            drawModeSelect(pomodoro.getModeDisplayName());
        } else {
            char buf[6];
            pomodoro.getTimeDisplay(buf, sizeof(buf));
            drawTimer(buf);
        }
    } else {
        // 现有时钟/日期/电量逻辑
        // 新增: BtnA 长按 3s → pomodoro.enter()
    }

    // BtnA+BtnB 5s 配置模式（保持不变）
}
```

## 显示

### 模式选择页（ModeSelect）

显示模式名称（如 `NORMAL`），使用现有 5×5 点阵字体，水平居中于 48 列。复用 `drawBattery` 的居中计算逻辑。

### 计时页（Running / Paused）

显示 `MM:SS` 倒计时（如 `25:00`），使用现有字体和冒号，水平居中。暂停时画面静止不动。

## 蜂鸣提示

阶段自然结束（倒计时到 0）时播放短 beep：`M5.Speaker.tone(1000, 200)`（1kHz, 200ms）。

长按 BtnA 跳过阶段时不蜂鸣（用户主动操作，不需要提醒）。

## 不包含

- 状态/阶段指示文字（Work/Break 显示）— 暂不实现
- 轮次显示（1/4）— 暂不实现
- 统计数据持久化 — 不在此版本范围内
