# STM32 Power Window ECU Prototype

STM32F446RE와 FreeRTOS를 기반으로 구현한 Power Window ECU 프로토타입입니다.

버튼 입력에 따른 Window UP/DOWN 제어, PWM 기반 DC Motor 제어,
INA219 전류 센싱, Moving Average 기반 전류 필터링,
Baseline Learning 및 Adaptive Threshold 기반 Anti-Pinch 기능을 구현했습니다.

또한 요구사항(Requirement) → 설계(Design) → 구현(Implementation) → Test Case → Test Result의
Traceability를 간단한 형태로 구성하고, UART 로그를 Python으로 파싱하여
Test Case별 PASS / FAIL을 자동 판정하는 Automated Verification Tool을 구현했습니다.

---

## 1. Project Overview

### Goal

단순 모터 구동을 넘어 실제 차량 전자제어기 개발/검증 흐름을 참고하여 다음 기능을 구현했습니다.

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
- Requirement 기반 Test Case 정의
- Requirement ↔ Test Case Traceability
- UART LOG 기반 Python Automated Verification Tool
- Test Case별 PASS / FAIL 자동 판정
- CSV Test Report 생성

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

## 6. Requirements

본 프로젝트에서는 구현된 기능을 기준으로 최소 단위의 Software Requirement를 정의하고,
각 Requirement를 Test Case와 연결했습니다.

> Note: 본 문서는 A-SPICE 공식 산출물이 아니라, Requirement-based Development / Verification 흐름을 학습하기 위한 프로젝트 수준의 간소화된 요구사항입니다.

| Requirement ID | Requirement |
|---|---|
| REQ-PW-001 | 운전자가 UP 동작을 요청하면 Window ECU는 Motor를 UP 방향으로 구동해야 한다. |
| REQ-PW-002 | 운전자가 DOWN 동작을 요청하면 Window ECU는 Motor를 DOWN 방향으로 구동해야 한다. |
| REQ-PW-003 | Window가 동작 중일 때 정지 명령이 입력되면 ECU는 Motor를 정지하고 `WINDOW_IDLE` 상태로 전이해야 한다. |
| REQ-PW-004 | Motor 기동 후 300 ms 이내의 Startup Current는 Anti-Pinch 판단에 사용하지 않아야 한다. |
| REQ-PW-005 | 정상 UP 동작 중 Filtered Current가 Adaptive Threshold 미만이면 Anti-Pinch Event를 발생시키지 않아야 한다. |
| REQ-PW-006 | Threshold를 일시적으로 1회 초과한 뒤 정상 범위로 복귀하면 Anti-Pinch Event를 발생시키지 않아야 한다. |
| REQ-PW-007 | UP 동작 중 Filtered Current가 Adaptive Threshold를 연속 3회 초과하면 ECU는 `WINDOW_CMD_ANTI_PINCH` Event를 발생시켜야 한다. |
| REQ-PW-008 | Anti-Pinch Event가 발생하면 ECU는 `WINDOW_ANTI_PINCH_REVERSE` 상태로 전이하고 Motor를 DOWN 방향으로 구동해야 한다. |
| REQ-PW-009 | Anti-Pinch Reverse가 시작된 후 1000 ms가 경과하면 ECU는 Motor를 정지하고 `WINDOW_IDLE` 상태로 복귀해야 한다. |
| REQ-PW-010 | Startup Blanking 이후 정상 운전 구간의 Filtered Current를 이용하여 Baseline Current를 학습해야 한다. |
| REQ-PW-011 | Anti-Pinch Threshold는 `Baseline Current + 10 mA`로 계산되어야 한다. |

---

## 7. Requirement Traceability

Requirement → Design → Implementation → Test Case → Test Result 흐름을 다음과 같이 구성했습니다.

```text
Requirement
    ↓
Software Design
    ↓
Implementation
    ↓
Test Case
    ↓
UART Log / Test Evidence
    ↓
Automated PASS / FAIL
```

### Traceability Matrix

| Requirement ID | Design / Implementation | Test Case | Result |
|---|---|---|---|
| REQ-PW-001 | `WINDOW_MANUAL_UP`, `Motor_RunUp()` | TC-01 | PASS |
| REQ-PW-002 | `WINDOW_MANUAL_DOWN`, `Motor_RunDown()` | TC-02 | PASS |
| REQ-PW-003 | `WINDOW_IDLE`, `Motor_Stop()` | TC-03 | PASS |
| REQ-PW-004 | `ANTI_PINCH_BLANKING_MS = 300` | TC-04 | PASS |
| REQ-PW-005 | Filtered Current / Adaptive Threshold 비교 | TC-05 | PASS |
| REQ-PW-006 | Consecutive Count Reset Logic | TC-06 | PASS |
| REQ-PW-007 | `ANTI_PINCH_COUNT_LIMIT = 3` / Queue Event | TC-07 | PASS |
| REQ-PW-008 | `WINDOW_ANTI_PINCH_REVERSE` | TC-08 | PASS |
| REQ-PW-009 | `ANTI_PINCH_REVERSE_MS = 1000` | TC-09 | PASS |
| REQ-PW-010 | Baseline Learning Logic | TC-10 | PASS |
| REQ-PW-011 | `baselineCurrent + CURRENT_DELTA_THRESHOLD_X10` | TC-10 + Log Evidence | PASS |

---

## 8. Anti-Pinch Algorithm

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

```c
#define ANTI_PINCH_BLANKING_MS 300
```

### Step 2. Moving Average Filter

```c
#define CURRENT_FILTER_SIZE 5
```

### Step 3. Baseline Current Learning

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

```text
Threshold = Baseline Current + Current Delta
```

```c
#define CURRENT_DELTA_THRESHOLD_X10 100
```

즉,

```text
Threshold = Baseline + 10mA
```

### Step 5. Consecutive Sample Detection

```c
#define ANTI_PINCH_COUNT_LIMIT 3
```

단일 Current Spike가 아니라 Threshold를 연속 3회 초과할 때 Anti-Pinch Event를 발생시킵니다.

---

## 9. Anti-Pinch Recovery

Anti-Pinch가 감지되면 CurrentTask가 직접 Motor를 제어하지 않고
`WINDOW_CMD_ANTI_PINCH` Command를 Queue에 전달합니다.

```text
WINDOW_MANUAL_UP
       ↓
WINDOW_CMD_ANTI_PINCH
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

<img width="654" height="474" alt="스크린샷 2026-09-06 144059" src="https://github.com/user-attachments/assets/d08fb272-72a2-4ddd-a61e-5fafd5f551d6" />

---

## 10. Verification Strategy

검증 항목은 정상 동작 확인에 그치지 않고 정상 상태 / 과도 상태 / 이상 상태로 나누어 구성했습니다.

- Functional Test
- Transient / Robustness Test
- Safety Logic Test
- Calibration / Learning Test

---

## 11. Test Cases

| TC ID | Test Objective | Preconditions | Test Input / Condition | Expected Result | Result |
|---|---|---|---|---|---|
| TC-01 | Manual UP 정상 동작 검증 | Window=IDLE | 버튼 입력 후 UP 명령 | Motor UP 구동, State=`MANUAL_UP` | PASS |
| TC-02 | Manual DOWN 정상 동작 검증 | Window=IDLE, nextDirection=DOWN | 버튼 입력 | Motor DOWN 구동, State=`MANUAL_DOWN` | PASS |
| TC-03 | 동작 중 정지 검증 | Window=`MANUAL_UP` 또는 `MANUAL_DOWN` | 버튼 재입력 | Motor 정지, State=`IDLE` | PASS |
| TC-04 | Startup Current 오검출 방지 | Window=`MANUAL_UP` 진입 | 모터 기동 직후 0~300ms 전류 증가 | Anti-Pinch Event 미발생 | PASS |
| TC-05 | Normal Current 정상 유지 검증 | Baseline Learning 완료 | 정상 부하에서 UP 지속 | Filtered Current가 Threshold 미만, Count=0 유지 | PASS |
| TC-06 | 단일 Current Spike 내성 검증 | Baseline Ready | Threshold 1회 초과 후 정상 복귀 | Count reset, Anti-Pinch 미발생 | PASS |
| TC-07 | 연속 과전류 Anti-Pinch 검출 | Baseline Ready, UP 진행 중 | Filtered Current가 Adaptive Threshold를 연속 3회 초과 | `WINDOW_CMD_ANTI_PINCH` Queue 전송 | PASS |
| TC-08 | Anti-Pinch Reverse 상태전이 검증 | Anti-Pinch Event 발생 | Queue Event 수신 | State=`ANTI_PINCH_REVERSE`, Motor Reverse 시작 | PASS |
| TC-09 | Reverse Recovery 완료 검증 | `ANTI_PINCH_REVERSE` 상태 | 1000ms 경과 | Motor 정지, State=`IDLE` | PASS |
| TC-10 | Baseline Learning 검증 | `MANUAL_UP` 진입 | 300~1300ms 정상 전류 측정 | Baseline 평균값 계산 및 `baselineReady=1` 전환 | PASS |

---

## 12. Automated Verification Tool

UART 로그를 Python으로 파싱하여 각 Test Case의 Expected Result와 실제 동작을 비교하고,
PASS / FAIL을 자동 판정하도록 검증 도구를 구현했습니다.

### Verification Flow

```text
STM32 UART Log
      ↓
Python Log Parser
      ↓
Current / Baseline / Threshold / Count / State Parsing
      ↓
Test Condition Evaluation
      ↓
PASS / FAIL
      ↓
CSV Test Report
```

### Automated Check Items

- TC-04: Startup / Learning 구간 Anti-Pinch 오검출 여부
- TC-05: 정상 부하에서 Anti-Pinch Count=0 유지 여부
- TC-06: 단일 Current Spike 이후 Count Reset 여부
- TC-07: 연속 과전류 이후 Anti-Pinch Event 발생 여부
- TC-08: Anti-Pinch Event 이후 Reverse 상태 전이 여부
- TC-09: Reverse 이후 IDLE 복귀 여부
- TC-10: Baseline Learning 완료 여부

### Example Automated Verification Result

```text
================================
 Power Window Verification
================================
TC-04 | PASS | Startup/learning 구간 Anti-Pinch 오검출 방지
TC-05 | PASS | 정상 부하에서 Anti-Pinch Count=0 유지
TC-06 | PASS | 단일 Current Spike 내성
TC-07 | PASS | 연속 과전류 Anti-Pinch Event 발생
TC-08 | PASS | Anti-Pinch Event 이후 Reverse 상태 전이
TC-09 | PASS | Anti-Pinch Reverse 이후 IDLE 복귀
TC-10 | PASS | Baseline Learning 완료
--------------------------------
Detected Baseline: 68.5 mA
```

### CSV Test Report

```text
TC_ID,Objective,Result
TC-04,Startup/learning 구간 Anti-Pinch 오검출 방지,PASS
TC-05,정상 부하에서 Anti-Pinch Count=0 유지,PASS
TC-06,단일 Current Spike 내성,PASS
TC-07,연속 과전류 Anti-Pinch Event 발생,PASS
TC-08,Anti-Pinch Event 이후 Reverse 상태 전이,PASS
TC-09,Anti-Pinch Reverse 이후 IDLE 복귀,PASS
TC-10,Baseline Learning 완료,PASS
```

테스트 조건이 로그에 존재하지 않는 경우에는 무조건 PASS로 처리하지 않고,
`NOT_TESTED`로 분리할 수 있도록 설계 방향을 잡았습니다.

---

<img width="951" height="255" alt="스크린샷 2026-09-06 165848" src="https://github.com/user-attachments/assets/0808f276-025c-4356-9a10-45cafc467253" />


## 13. Representative Test Evidence

실제 Motor 부하 시험에서 다음과 같은 결과를 확인했습니다.

```text
Baseline = 72.4 mA
Threshold = 82.4 mA
```

정상 상태에서는 Filtered Current가 Threshold 미만으로 유지되며 `Count=0`을 유지했습니다.

부하 증가 시:

```text
[CURRENT] Raw=92.3, Filt=84.5, Base=72.4, Th=82.4
[CURRENT] Raw=79.5, Filt=83.4, Base=72.4, Th=82.4, Count=1
[CURRENT] Raw=79.8, Filt=83.6, Base=72.4, Th=82.4, Count=2
[ANTI-PINCH] EVENT QUEUED
[WINDOW] ANTI_PINCH_REVERSE
[WINDOW] IDLE
```

이를 통해 다음 전체 흐름을 검증했습니다.

```text
Load Increase
    ↓
Current Increase
    ↓
Moving Average Filtering
    ↓
Adaptive Threshold Detection
    ↓
Consecutive Sample Validation
    ↓
Queue Event
    ↓
State Transition
    ↓
Reverse
    ↓
Idle
```

---

## 14. Debugging & Engineering Findings

### Startup Current False Positive
초기 Startup Current가 Threshold를 초과하여 Anti-Pinch로 오검출되는 문제가 발생했습니다.

해결:
- Startup Blanking 적용
- Moving Average Filter Reset

### Moving Average History Problem
Startup 구간을 Detection에서 제외했더라도 Startup Current가 Filter Buffer에 남아 이후 Detection에 영향을 주는 문제가 있었습니다.

해결:
- Blanking 구간 동안 Filter 미적용
- `MANUAL_UP` 진입 시 Filter Reset

### Fixed Threshold Limitation
Normal Current가 Motor Load 및 PWM Duty에 따라 변화하여 Fixed Threshold 방식에서 False Positive가 발생했습니다.

해결:
- Fixed Threshold → Baseline Learning → Adaptive Threshold

### Manual Verification Limitation
UART 로그를 사람이 직접 읽으며 Test Case를 판정할 경우 반복 시험에서 동일한 판정 기준을 유지하기 어렵고 결과 정리에 시간이 소요됐습니다.

해결:
- Python 기반 Log Parser 구현
- Expected Condition 기반 자동 판정
- Test Case별 PASS / FAIL Report 생성
- CSV 결과 저장

---

## 15. Development & Verification Traceability

본 프로젝트에서는 다음 흐름을 프로젝트 수준에서 직접 구성했습니다.

```text
Requirement
    ↓
Design
    ↓
Implementation
    ↓
Test Case
    ↓
Test Evidence
    ↓
Automated Test Result
```

이를 통해 단순 기능 구현보다,
요구사항이 어떤 SW 설계와 구현으로 연결되고 어떤 Test Case로 검증되는지를 추적하는
Requirement-based Development / Verification 방식을 실습했습니다.

---

## 16. Verification-Oriented Learning

본 프로젝트를 통해 다음 역량을 실습했습니다.

- Requirement 정의
- Requirement ↔ Test Case Traceability
- Requirement-based Test Case 설계
- 정상 / 과도 / 이상 조건 분리
- UART LOG 기반 동작 분석
- Python 기반 LOG Parsing
- Expected Result 기반 자동 판정
- CSV Test Report 생성
- False Positive 분석
- Calibration Parameter 조정
- State Transition 검증
- Event-driven SW Verification
- Verification Automation

---

## 17. Future Improvements

- AUTO UP / AUTO DOWN
- Upper / Lower Limit Switch
- Motor Stall Timeout
- INA219 Communication Fault Injection Test
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
- TC-01 ~ TC-03 자동 판정 확대
- 실시간 Serial Log Capture 자동화
- Automated Regression Test
- Fault Injection Test
- Test Result Visualization / Plot
- Requirement ↔ Test Case ↔ Result Traceability 자동화

---

## 18. Tech Stack

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

### Verification / Automation
- Python
- Regular Expression
- UART Log Parsing
- CSV Test Report
- Automated PASS / FAIL Evaluation
- Requirement Traceability

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

## 19. Summary

STM32F446RE·FreeRTOS 기반 Power Window ECU 프로토타입을 구현하고,
INA219 전류 센싱과 Moving Average, Startup Blanking, Baseline Learning을 적용한
Adaptive Threshold 기반 Anti-Pinch Detection Logic을 개발했습니다.

또한 구현 기능을 11개의 Requirement로 정의하고,
각 Requirement를 Design / Implementation / Test Case / Test Result와 연결하여
프로젝트 수준의 Traceability를 구성했습니다.

요구 기능은 정상 / 과도 / 이상 조건으로 분해하여 10개 Test Case로 검증했으며,
Python 기반 Automated Verification Tool을 구현해 UART 로그에서
Baseline, Threshold, Current, Count, State 정보를 파싱하고 Test Case별 PASS / FAIL을 자동 판정했습니다.

최종 검증 결과는 CSV Test Report로 생성하여 반복 검증 결과를 동일한 기준으로 확인할 수 있도록 구성했습니다.
