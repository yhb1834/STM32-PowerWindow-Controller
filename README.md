# STM32 Power Window ECU Prototype

STM32F446RE와 FreeRTOS를 기반으로 구현한 Power Window ECU 프로토타입입니다.

버튼 입력에 따른 Window UP/DOWN 제어, PWM 기반 DC Motor 제어,
INA219 전류 센싱, Moving Average 기반 전류 필터링,
Baseline Learning 및 Adaptive Threshold 기반 Anti-Pinch 기능을 구현했습니다.

---

## 1. Project Overview

### Goal

단순 모터 구동을 넘어 실제 차량 전자제어기 구조를 참고하여 다음 기능을 구현했습니다.

- Power Window UP / DOWN 제어
- State Machine 기반 Window 상태 관리
- FreeRTOS Task 기반 기능 분리
- Message Queue 기반 Event 전달
- PWM 기반 DC Motor 제어
- INA219 기반 Motor Current Monitoring
- Startup Current Blanking
- Moving Average Filtering
- Baseline Current Learning
- Adaptive Threshold 기반 Anti-Pinch Detection
- Anti-Pinch 발생 시 Reverse Recovery

---

## 2. Hardware

### MCU
- STM32F446RE
- NUCLEO-F446RE

### Motor Driver
- L298N

### Current Sensor
- INA219
- I2C Communication
- R100 Shunt Resistor

### Motor
- 12V Geared DC Motor

### Power
- External 12V Power Supply

---

## 3. Software Architecture

FreeRTOS 기반으로 각 기능을 Task 단위로 분리했습니다.

```text
                     +----------------+
                     |   InputTask    |
                     | Button Polling |
                     +--------+-------+
                              |
                              | WINDOW_CMD_TOGGLE
                              v
                    +---------------------+
                    | windowCommandQueue  |
                    +----------+----------+
                               |
                               v
                    +---------------------+
                    |    ControlTask      |
                    |   State Machine     |
                    +----------+----------+
                               |
                               v
                       Motor Control
                               ^
                               |
                    +----------+----------+
                    |   CurrentTask       |
                    | INA219 Monitoring   |
                    | Anti-Pinch Detect   |
                    +----------+----------+
                               |
                               | WINDOW_CMD_ANTI_PINCH
                               +----------> Queue
```

---

## 4. FreeRTOS Tasks

### InputTask

사용자 버튼 입력을 주기적으로 확인하고 Window Command를 Queue에 전달합니다.

```text
Button Input
    ↓
WINDOW_CMD_TOGGLE
    ↓
windowCommandQueue
```

### ControlTask

Queue에서 Command를 수신하여 Window State를 변경하고 Motor Control을 수행합니다.

주요 상태:

```c
WINDOW_IDLE
WINDOW_MANUAL_UP
WINDOW_MANUAL_DOWN
WINDOW_AUTO_UP
WINDOW_AUTO_DOWN
WINDOW_ANTI_PINCH_REVERSE
WINDOW_FAULT
```

### CurrentTask

INA219를 통해 Motor Current를 주기적으로 측정하고 Anti-Pinch Detection Logic을 수행합니다.

Sampling Period:

```text
50 ms
```

---

## 5. Motor Control

TIM3 PWM을 이용하여 Motor Speed를 제어합니다.

PWM Frequency:

```text
20 kHz
```

Motor Driver Direction Control:

```text
UP
IN1 = HIGH
IN2 = LOW

DOWN
IN1 = LOW
IN2 = HIGH
```

---

## 6. Anti-Pinch Algorithm

초기에는 Fixed Current Threshold 방식으로 구현했습니다.

```c
if (current >= 80mA)
{
    overCurrentCount++;
}
```

하지만 실제 테스트 과정에서 다음 문제가 발생했습니다.

- PWM Duty 변화에 따라 측정되는 Current Level 변화
- Startup Current Spike
- Normal Current Variation
- Fixed Threshold에 의한 False Positive

이를 개선하기 위해 Anti-Pinch Detection을 단계적으로 발전시켰습니다.

### Step 1. Startup Blanking

Motor Start 직후 발생하는 높은 Startup Current를 Anti-Pinch로 오검출하지 않도록 일정 시간 동안 Detection을 비활성화했습니다.

```c
#define ANTI_PINCH_BLANKING_MS 300
```

### Step 2. Moving Average Filter

INA219 Current Signal의 순간적인 변동을 줄이기 위해 최근 5개의 Sample을 평균내는 Moving Average Filter를 적용했습니다.

```c
#define CURRENT_FILTER_SIZE 5
```

### Step 3. Baseline Current Learning

Motor의 정상 운전 전류를 고정값으로 가정하지 않고,
Motor Start 후 정상 운전 구간의 Filtered Current를 학습하여 Baseline을 생성합니다.

```text
0 ~ 300 ms
Startup Blanking

300 ~ 1300 ms
Baseline Learning

1300 ms ~
Anti-Pinch Detection
```

```c
#define BASELINE_LEARNING_MS 1000
```

### Step 4. Adaptive Threshold

Anti-Pinch Threshold를 고정값 대신 다음과 같이 결정합니다.

```text
Threshold = Baseline Current + Current Delta
```

현재 Calibration:

```c
#define CURRENT_DELTA_THRESHOLD_X10 100
```

즉,

```text
Threshold = Baseline + 10mA
```

예시:

```text
Baseline = 72.4mA
Threshold = 82.4mA
```

### Step 5. Consecutive Sample Detection

단일 Current Spike에 의해 Anti-Pinch가 발생하지 않도록
Threshold를 연속 3회 초과할 경우에만 Event를 발생시킵니다.

```c
#define ANTI_PINCH_COUNT_LIMIT 3
```

---

## 7. Anti-Pinch Recovery

Anti-Pinch가 감지되면 CurrentTask가 직접 Motor를 제어하지 않고
`WINDOW_CMD_ANTI_PINCH` Command를 Queue에 전달합니다.

```text
WINDOW_MANUAL_UP
       ↓
WINDOW_ANTI_PINCH_REVERSE
       ↓
Reverse Motor
       ↓
1000 ms
       ↓
WINDOW_IDLE
```

```c
#define ANTI_PINCH_REVERSE_MS 1000
```

---

## 8. Test Result

실제 Motor 부하 시험에서 다음과 같은 결과를 확인했습니다.

```text
Baseline = 72.4 mA
Threshold = 82.4 mA
```

Motor Shaft에 부하를 증가시키면:

```text
Raw=92.3 mA
Filtered=84.5 mA

Raw=79.5 mA
Filtered=83.4 mA
Count=1

Raw=79.8 mA
Filtered=83.6 mA
Count=2

[ANTI-PINCH] EVENT QUEUED
[WINDOW] ANTI_PINCH_REVERSE
[WINDOW] IDLE
```
<img width="654" height="474" alt="스크린샷 2026-09-06 144059" src="https://github.com/user-attachments/assets/f1231150-26e1-4bec-a92d-d1733d0772be" />


---

## 9. Debugging & Engineering Findings

### Startup Current False Positive

초기 Startup Current가 Threshold를 초과하여 Anti-Pinch로 오검출되는 문제가 발생했습니다.

해결:
- Startup Blanking 적용
- Moving Average Filter Reset

### Moving Average History Problem

Startup 구간을 Detection에서 제외했더라도 Startup Current가 Filter Buffer에 남아 이후 Detection에 영향을 주는 문제가 있었습니다.

해결:
- Blanking 구간 동안 Filter 미적용
- MANUAL_UP 진입 시 Filter Reset

### Fixed Threshold Limitation

Normal Current가 Motor Load 및 PWM Duty에 따라 변화하여 Fixed Threshold 방식에서 False Positive가 발생했습니다.

해결:
- Fixed Threshold → Baseline Learning → Adaptive Threshold

---

## 10. Current Detection Logic

```text
Motor UP Start
     ↓
Startup Blanking
     ↓
Moving Average
     ↓
Baseline Learning
     ↓
Baseline Ready
     ↓
Threshold = Baseline + 10mA
     ↓
Filtered Current > Threshold?
     ↓ Yes
Consecutive Count++
     ↓
Count >= 3?
     ↓ Yes
WINDOW_CMD_ANTI_PINCH
     ↓
Message Queue
     ↓
ControlTask
     ↓
ANTI_PINCH_REVERSE
```

---

## 11. Future Improvements

- AUTO UP / AUTO DOWN
- Upper / Lower Limit Switch
- Motor Stall Timeout
- INA219 Communication Fault Handling
- Sensor Plausibility Check
- I2C Mutex
- UART Logging Mutex
- Runtime Baseline Tracking
- Current Gradient Detection
- Position / Speed Sensor Fusion
- CAN Communication
- Diagnostic Trouble Code
- Watchdog
- NVM Calibration Storage

---

## 12. Tech Stack

### Embedded
- STM32F446RE
- STM32CubeIDE
- STM32 HAL
- C

### RTOS
- FreeRTOS
- CMSIS-RTOS v2
- Task
- Message Queue

### Interface
- I2C
- UART
- PWM
- GPIO

### Hardware
- INA219
- L298N
- DC Geared Motor

---

## 13. Key Learning

본 프로젝트를 통해 단순 Motor Control뿐 아니라
실제 Embedded Control System에서 중요한 다음 개념을 실습했습니다.

- Real-Time Task Separation
- Event-driven Control
- State Machine
- Sensor Filtering
- Startup Transient Handling
- Calibration
- Fault Detection Logic
- Adaptive Threshold
- Hardware / Software Integration
