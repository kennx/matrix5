# 电池负载状态补偿设计 (Load-Aware Compensation)

## 概述

在前一版的电池准确性提升设计中，引入了 `BatteryEstimator` 来进行基准曲线映射和长期的电压锚点学习。然而，由于 M5StickS3 没有硬件库仑计，ESP32 仅通过 ADC 读取端电压（Voltage），电池的内阻效应导致电压在有负载时会发生明显压降（Voltage Drop）。
这使得当屏幕点亮或 WiFi 开启时，读取到的电压偏低，导致计算出的电量百分比下降；负载消除后，电压回弹，电量又显得“虚高”。

本设计的目的是引入**负载状态补偿（Load-Aware Compensation）**，通过评估当前设备的负载状态（屏幕亮度、WiFi 状态），在软件层面把内阻导致的压降加回到 ADC 读数中，从而还原出近似的电池开路电压（Open Circuit Voltage, OCV）。所有的平滑和学习逻辑均基于这个补偿后的 OCV 进行，从而使得电量显示不再受瞬时负载变化的影响。

## 目标

- 消除屏幕背光亮度变化带来的电量百分比跳动
- 消除 WiFi 扫描/连接时大电流造成的电压跌落导致的虚假放电
- 使得 `BatteryEstimator` 学习到的高低电量锚点真正反映静息电压（OCV），不受日常使用习惯（比如一直开着高亮度）的污染

## 补偿模型

电池端电压 $V_{meas} = OCV - I_{load} \times R_{internal}$。
因为无法直接测量电流 $I_{load}$ 且不确切知道 $R_{internal}$，我们采用硬编码的经验压降常数来替代 $I_{load} \times R_{internal}$。

计算公式：
`compensatedMv = rawVoltageMv + (backlightPercent * MAX_BL_DROP_MV / 100) + (wifiActive ? WIFI_DROP_MV : 0)`

### 预估参数（可调）

- **`MAX_BL_DROP_MV`**: 预设为 `40` (mV)。代表屏幕亮度为 100% 时的压降。
- **`WIFI_DROP_MV`**: 预设为 `50` (mV)。代表 WiFi 射频工作（STA 模式下连接或扫描）时的压降。
注：当设备处于充电状态时，如果内部电源管理路径绕过了电池直接供电（或者充电电流改变了模型），补偿可能不准确。但由于充电时电压本就被拉高，且算法中对充电状态有 `单调不降` 的强力保护，因此充电状态下可以不做补偿，或应用相同的补偿逻辑。为保持模型简单，本版对充电与放电统一应用补偿公式。

## 架构变更

### 1. `BatterySample` 结构扩展

```cpp
struct BatterySample {
    int voltageMv;
    bool charging;
    unsigned long timestampMs;
    uint8_t backlightPercent; // 0-100，当前屏幕亮度
    bool wifiActive;          // true: WiFi 开启并处于耗电状态
};
```

### 2. `BatteryEstimator` 处理逻辑

在 `BatteryEstimator::update()` 的最开头，立刻计算出 `compensatedMv`。
随后，将原本赋给 `filteredVoltage_` 的逻辑全部替换为对 `compensatedMv` 的低通滤波：
- `filteredVoltage_` 现在代表“平滑后的补偿开路电压”，而不是“平滑后的 ADC 电压”。
- 所有的基线曲线映射、满电锚点学习、低电锚点学习、放电速率学习，全部自动享受 OCV 级别的数据稳定性。

### 3. `main.cpp` 采集端适配

在 `refreshBatteryEstimate` 函数中：
- 亮度：读取全局配置 `cfgData.brightness`（或者当前实际应用的背光占空比）。
- WiFi：通过 `WiFi.getMode() == WIFI_STA` 结合当前是否连接 `WiFi.status() == WL_CONNECTED` 或扫描状态 `wifiScanInProgress` 来判断。
将这两个状态填入 `BatterySample` 并传递给 `BatteryEstimator`。

## 防止异常的保护机制

- **补偿阈值上限**：硬件最大的总补偿压降不应超过 100mV，以防计算过度。
- **配置切换去抖**：在状态突变（例如瞬间开关 WiFi 或调整屏幕亮度）时，ADC 电压可能存在几百毫秒的滞后，而软件状态是瞬间更新的。原有的 EMA 滤波器（`filteredVoltage_ = filteredVoltage_ * 0.8 + compensatedMv * 0.2`）恰好能自然地平滑这种突变，因此无需额外增加状态去抖定时器。

## 验证与测试

1. **亮度突变测试**：在番茄钟界面，如果发生屏幕亮度衰减，电池百分比不应发生跳变。
2. **配置模式测试**：同时按住 BtnA+BtnB 进入配置模式时，会触发 WiFi 扫描。此时原本电压会被大幅拉低，补偿后预期电压保持平稳。
3. **长期学习测试**：运行一段时间后，输出的 `learnedFullMv` 应当稳定在 4100~4180mV 之间（反映真实满电电压），而不是因为高亮屏幕被拉低到 4050mV 以下。

