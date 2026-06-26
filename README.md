# 센서 융합 기반 반자율 주행 RC 시스템

> Arduino 기반 센서 융합, 장애물 회피 자율주행, Bluetooth 수동 제어를 결합한 교육용 반자율 RC카 프로젝트

## 프로젝트 개요

이 프로젝트는 전기자동차(EV)에 적용되는 **센서 - MCU - 제어 로직 - 구동부**의 구조를 단순화하여 RC카 형태로 구현한 교육용 반자율 주행 시스템입니다.

초음파 센서와 적외선 센서로 전방 및 측면의 장애물을 감지하고, Arduino가 센서 값을 기반으로 주행·회피·긴급 정지를 판단합니다. 사용자는 Android 애플리케이션과 Bluetooth Classic 통신을 이용해 자율주행과 수동 조종 모드를 전환할 수 있습니다.

이 저장소에는 최종 RC카 프로젝트뿐 아니라 부트캠프 과정에서 수행한 로봇팔 및 컬러 분류 컨베이어 실습 코드도 함께 보존되어 있습니다.

## 프로젝트 정보

- 프로젝트명: 센서 융합 기반 반자율 주행 시스템
- 팀명: Unlucky
- 수행 시기: 2026년 01월 26일 ~ 2026년 2월 6일
- 수행 과정: Semi-Auto Arduino Bootcamp
- 발표 행사: 한남대학교 2026 제1회 Semi-COSS 페스티벌
- 프로젝트 형태: 2인 팀 프로젝트
- 주요 분야: Embedded System, Sensor Fusion, Bluetooth Communication, Mobile Application

## 프로젝트 목적

- 전기자동차에 적용되는 반도체 기반 주행 제어 구조를 단순화하여 구현
- 센서와 MCU가 실제 물리 시스템을 제어하는 과정을 실습
- 자율 제어와 사용자 수동 제어가 공존하는 반자율 구조 설계
- 위험 상황에서 자동 정지와 사용자 개입이 가능한 안전 제어 구조 구현
- 기존 로봇팔·컨베이어 자동화 실습을 이동형 로봇 시스템으로 확장

## 주요 기능

### 자율주행 모드

- 초음파 센서를 이용한 전방 장애물 거리 측정
- 좌·우 적외선 센서를 이용한 측면 및 근접 장애물 감지
- 센서 감지 방향에 따른 회피 방향 결정
- 장애물 접근 시 후진 및 회전 동작 수행
- 위험 거리 진입 시 긴급 정지 및 복구 시퀀스 실행

### 수동 조종 모드

- Android 앱에서 전진·후진·좌회전·우회전·정지 제어
- Bluetooth Classic SPP를 이용한 무선 명령 전송
- 버튼을 누르는 동안 이동하고 손을 떼면 정지
- 앱에서 MANUAL/AUTO 모드 전환

### 주행 안정화

- 수동 조작 시 모터 출력을 단계적으로 변화시키는 가감속 램프 적용
- Bluetooth 명령이 일정 시간 동안 들어오지 않으면 자동 정지
- 초음파 기반 긴급 정지는 수동·자율 모드보다 우선 처리
- NeoPixel LED를 이용한 모드 및 위험 상태 표시
- 부저를 이용한 시작 및 경고음 출력

## 시스템 구성

```text
[초음파 센서] ─┐
                ├─> [Arduino UNO] ─> [모터 드라이버] ─> [좌·우 DC 모터]
[적외선 센서] ─┘          │
                           ├─> [NeoPixel LED]
                           ├─> [Buzzer]
                           │
[Android App] <─Bluetooth─> [HC-05/HC-06 계열 모듈]
```

시스템은 다음 기능 블록으로 구성됩니다.

| 구분 | 역할 |
|---|---|
| 환경 인식부 | 초음파·적외선 센서를 이용한 전방 및 측면 장애물 감지 |
| 중앙 판단부 | Arduino에서 센서 데이터 처리, 주행 모드 및 회피 동작 결정 |
| 주행 구동부 | 모터 드라이버와 좌·우 DC 모터를 이용한 차동 구동 |
| 상태 출력부 | NeoPixel LED와 부저를 이용한 상태·위험 알림 |
| 사용자 인터페이스 | Android 앱을 이용한 연결, 모드 전환 및 방향 조작 |
| 무선 통신부 | Bluetooth Classic SPP를 이용한 제어 명령 송수신 |

## 하드웨어 구성

| 부품 | 용도 |
|---|---|
| Arduino UNO | 센서 입력 처리 및 전체 주행 제어 |
| 모터 드라이버 쉴드 | 좌·우 DC 모터의 방향 및 PWM 속도 제어 |
| HC-SR04 초음파 센서 | 전방 장애물 거리 측정 |
| 적외선 센서 2개 | 좌·우 근접 장애물 감지 |
| DC 기어 모터 | 차량 이동 및 차동 조향 |
| HC-05/HC-06 계열 Bluetooth 모듈 | Android 앱과 시리얼 통신 |
| NeoPixel LED 모듈 | 모드 및 위험 상태 표시 |
| Buzzer | 시작음 및 경고음 출력 |
| RC카 섀시 및 전원부 | 부품 장착과 시스템 전원 공급 |

## RC카 핀 설정

`rc_car_v3_stable.ino`와 `rc_car_v4_ramp_control.ino`는 다음 핀 구성을 사용합니다.

| 기능 | Arduino 핀 |
|---|---:|
| 좌측 모터 방향 | D12 |
| 좌측 모터 PWM | D10 |
| 우측 모터 방향 | D13 |
| 우측 모터 PWM | D11 |
| NeoPixel LED | D5 |
| Buzzer | D4 |
| 초음파 TRIG | D7 |
| 초음파 ECHO | D8 |
| 왼쪽 적외선 센서 | A1 |
| 오른쪽 적외선 센서 | A2 |
| Bluetooth SoftwareSerial RX | D2 |
| Bluetooth SoftwareSerial TX | D3 |

> Bluetooth 모듈의 TX는 Arduino RX(D2), Bluetooth 모듈의 RX는 Arduino TX(D3)에 교차 연결합니다. 사용하는 모듈의 입력 전압 조건에 따라 분압 회로가 필요할 수 있습니다.

## 소프트웨어 구성

### Arduino Firmware

- 센서 데이터 수집
- 자동 장애물 회피 상태 머신
- 위험 거리 긴급 정지 및 복구
- 수동 주행 명령 처리
- NeoPixel 및 부저 상태 출력
- SoftwareSerial 기반 Bluetooth 통신

### Android Application

- Java 기반 Android 앱
- Bluetooth Classic SPP 연결
- 페어링된 장치 이름을 기준으로 연결
- MANUAL/AUTO 모드 전환
- 게임패드 형태의 방향 버튼 제공
- Android 12 이상 `BLUETOOTH_CONNECT` 권한 처리

앱 코드에 설정된 Bluetooth 장치 이름은 다음과 같습니다.

```java
private static final String TARGET_DEVICE_NAME = "edu14";
```

실제 Bluetooth 모듈 이름이 다르면 `MainActivity.java`의 값을 수정해야 합니다.

## Bluetooth 명령 규격

Android 앱은 명령 뒤에 줄바꿈(`\n`)을 붙여 전송합니다. 문자열 명령은 코드에서 `_`를 종료 문자로도 처리합니다.

| 명령 | 기능 |
|---|---|
| `manual_` | 수동 조종 모드 진입 |
| `start_` | 자율주행 모드 시작 |
| `stop_` | 전체 정지 요청 |
| `F` | 전진 |
| `B` | 후진 |
| `L` | 제자리 좌회전 |
| `R` | 제자리 우회전 |
| `S` | 수동 주행 정지 |

`rc_car_v3_stable.ino`는 다음 추가 명령도 지원합니다.

| 명령 | 기능 |
|---|---|
| `mstart_` | 수동 모드 시작 |
| `mstop_` | 수동 모드 종료 및 대기 상태 진입 |
| `V0` ~ `V255` | 수동 주행 속도 설정 |

## 동작 흐름

### 자율주행

```text
대기 상태
  -> AUTO 모드 시작
  -> 센서값 측정
  -> 정상 구간 전진
  -> 장애물 감지
  -> 정지·후진·회전 회피
  -> 위험 거리 진입 시 긴급 정지
  -> 안전 거리 확인 후 정상 주행 복귀
```

### 수동 조종

```text
앱 실행
  -> Bluetooth 활성화 및 모듈 페어링
  -> CONNECT
  -> MANUAL 모드 진입
  -> 방향 버튼 입력
  -> F/B/L/R 명령 전송
  -> 버튼에서 손을 떼면 S 전송
  -> 필요 시 AUTO 모드로 전환
```

## 저장소 구조

```text
.
├── README.md
├── HW/
│   ├── rc_car_v4_ramp_control/
│   │   └── rc_car_v4_ramp_control.ino
│   │
│   └── 수행과정/
│       ├── rc_car_v3_stable/
│       │   └── rc_car_v3_stable.ino
│       │
│       └── bootscamp/
│           ├── robot_arm_serial_control/
│           │   └── robot_arm_serial_control.ino
│           ├── robot_arm_bluetooth_eeprom/
│           │   └── robot_arm_bluetooth_eeprom.ino
│           └── color_sorting_conveyor/
│               └── color_sorting_conveyor.ino
│
└── SW/
    └── app/
        ├── build.gradle.kts
        ├── src/
        │   └── main/
        │       ├── AndroidManifest.xml
        │       ├── java/com/example/rc_controller/MainActivity.java
        │       └── res/
        └── build/
            └── outputs/apk/debug/app-debug.apk
```

> 원본 폴더명의 `bootscamp`는 부트캠프 실습 자료를 의미합니다. 일반적인 영문 표기는 `bootcamp`이지만, 기존 경로와 파일 이력을 보존하기 위해 현재 이름을 유지했습니다.

## RC카 펌웨어 버전

### `rc_car_v4_ramp_control`

가장 나중에 수정된 RC카 펌웨어입니다.

- 수동 주행 가속·감속 램프 적용
- 자율주행 및 장애물 회피
- 초음파 긴급 정지
- `start`, `manual`, `F/B/L/R/S` 명령 지원
- Android 앱의 중앙 STOP은 `stop_` 이후 전송되는 `S` 명령으로 실제 모터를 정지시킴

현재 저장소에서는 이 버전을 **최신 최종 후보**로 분류합니다.

### `rc_car_v3_stable`

앱 명령 규격이 더 명시적으로 구현된 이전 안정 버전입니다.

- `start`, `stop`, `manual`, `mstart`, `mstop` 지원
- `F/B/L/R/S` 수동 주행 지원
- `V0`~`V255` 속도 변경 지원
- 수동 명령 타임아웃 자동 정지
- 코드 구조와 주석이 비교적 상세함

앱과의 명령 대응을 명확하게 확인하거나 기능을 수정할 때 참고하기 적합합니다.

## 부트캠프 수행 과정

### 1. Robot Arm Serial Control

`HW/수행과정/bootscamp/robot_arm_serial_control/`

- 서보모터 4개 제어
- 시리얼 명령 기반 관절별 각도 제어
- `arm0_90` 형식의 명령 파싱
- 목표 각도까지 단계적으로 이동

### 2. Robot Arm Bluetooth & EEPROM

`HW/수행과정/bootscamp/robot_arm_bluetooth_eeprom/`

- 조이스틱 및 Bluetooth 제어
- EEPROM에 최대 8개의 자세 저장
- 저장 자세 1회 또는 반복 재생
- `save`, `play`, `auto`, `stop`, `clear`, `list` 명령 지원
- 관절별 동작 범위 및 이동 속도 제한

### 3. Color Sorting Conveyor

`HW/수행과정/bootscamp/color_sorting_conveyor/`

- 적외선 센서로 제품 진입 감지
- TCS34725 컬러 센서로 색상 판별
- 서보모터를 이용한 색상별 분류
- DC 모터 기반 컨베이어 제어
- NeoPixel 및 부저를 이용한 결과 표시
- Bluetooth `start`/`stop` 명령 지원

이 실습들은 단일 센서·액추에이터 제어에서 시작해 저장 동작 자동화, 센서 판별, 무선 제어를 거쳐 최종 반자율 RC카 시스템으로 확장되는 학습 과정을 보여줍니다.

## 필요한 Arduino 라이브러리

Arduino IDE의 라이브러리 매니저에서 프로젝트에 따라 다음 라이브러리를 설치합니다.

| 라이브러리 | 사용 프로젝트 |
|---|---|
| Adafruit NeoPixel | RC카, 컬러 분류 컨베이어 |
| Adafruit TCS34725 | 컬러 분류 컨베이어 |
| Servo | 로봇팔, 컬러 분류 컨베이어 |
| EEPROM | 로봇팔 자세 저장 |
| SoftwareSerial | RC카, Bluetooth 로봇팔, 컨베이어 |
| Wire | 컬러 센서 I2C 통신 |

`Servo`, `EEPROM`, `SoftwareSerial`, `Wire`는 사용하는 Arduino AVR 보드 패키지에 기본 포함되는 경우가 많습니다.

## 실행 방법

### Arduino

1. Arduino IDE에서 사용할 `.ino` 파일과 같은 이름의 폴더를 엽니다.
2. 보드를 `Arduino UNO`로 선택합니다.
3. 필요한 라이브러리를 설치합니다.
4. 핀 배선과 전원 구성을 확인합니다.
5. 코드를 업로드합니다.
6. 필요하면 시리얼 모니터를 `9600 baud`로 열어 명령을 테스트합니다.

### Bluetooth

1. Bluetooth 모듈을 `9600 baud`로 설정합니다.
2. 스마트폰 설정에서 모듈을 먼저 페어링합니다.
3. 모듈 이름이 앱의 `TARGET_DEVICE_NAME`과 같은지 확인합니다.
4. 앱에서 `CONNECT`를 누릅니다.
5. MANUAL 또는 AUTO 모드를 선택합니다.

### Android 앱

현재 압축본에는 Android 앱의 `app` 모듈 소스와 과거 빌드 결과가 보존되어 있습니다.

- 앱 소스: `SW/app/src/main/`
- 과거 Debug APK: `SW/app/build/outputs/apk/debug/app-debug.apk`
- 최소 Android 버전: API 27
- Target SDK: API 34
- Java: 11

다만 저장소에는 프로젝트 최상위의 Gradle Wrapper, `settings.gradle(.kts)`, 버전 카탈로그 파일 등이 포함되어 있지 않습니다. Android Studio에서 다시 빌드하려면 새 프로젝트를 생성한 후 현재 `app` 모듈을 옮기거나, 누락된 프로젝트 수준 Gradle 구성을 복원해야 합니다.

또한 `SW/app/.gitignore`에 `/build`가 지정되어 있으므로 일반적인 `git add .` 과정에서는 Android 빌드 산출물이 Git에 포함되지 않습니다.

## 동작 검증 결과

발표 당시 다음 항목의 동작을 확인했습니다.

- 센서 인식 결과에 따른 자율주행 및 장애물 회피
- 위험 거리 접근 시 자동 정지
- 자율주행 중 수동 조작으로의 전환
- Bluetooth 기반 Android 앱 연결 및 제어
- 실시간 모터 출력 조절을 통한 주행 안정성 개선
- 위험 상황 종료 후 정상 주행 복귀

> 이 저장소는 당시 프로젝트 소스를 보존한 자료입니다. 현재 하드웨어에서의 재현 여부는 센서 방향, 모터 배선, 전원 상태, 임계값 및 Bluetooth 모듈 설정에 따라 달라질 수 있습니다.

## 조정 가능한 주요 값

`rc_car_v4_ramp_control.ino`의 다음 값을 실제 환경에 맞게 조정할 수 있습니다.

| 설정 | 기본값 | 의미 |
|---|---:|---|
| `IR_TH` | 500 | 적외선 센서 감지 임계값 |
| `DRIVE` | 120 | 자율주행 전진 속도 |
| `DETECT` | 15 cm | 장애물 회피 시작 거리 |
| `EMERG` | 6 cm | 긴급 정지 거리 |
| `CLEAR` | 12 cm | 복구 후 안전 판단 거리 |
| `MAX_V` | 250 | 수동 주행 최대 PWM |
| `ACC` | 6 | 램프 가속 증가량 |
| `DEC_STEP` | 10 | 램프 감속 감소량 |
| `MANUAL_TO` | 600 ms | 수동 명령 미수신 자동 정지 시간 |

적외선 센서가 장애물을 감지할 때 값이 증가하는 모듈이라면 다음 조건을 반대로 수정해야 합니다.

```cpp
#define IR_HIT(v) ((v) > IR_TH)
```

## 한계 및 개선 방향

- 초음파·적외선 센서의 재질, 각도, 주변광에 따른 오차 보정
- 전방·측면·후방 센서 배치 최적화를 통한 사각지대 감소
- ToF 또는 LiDAR 등 정밀 거리 센서 적용
- 차량 외형에 맞춘 배선 및 센서 모듈 소형화
- 모터별 편차를 보정하기 위한 속도 피드백 제어
- 앱에서 Bluetooth 장치 검색 및 선택 기능 추가
- 연결 해제 시 펌웨어와 앱의 상태 동기화 강화
- `rc_car_v4_ramp_control`에 `stop` 문자열 명령을 명시적으로 처리하는 로직 추가
- 완전한 Android Studio 프로젝트 구조 및 Release 빌드 구성 복원
- 실제 전기자동차 주행 환경을 반영한 복합 시나리오 확장

## 프로젝트 의의

이 프로젝트는 완전한 자율주행 알고리즘을 구현하는 데 목적을 두기보다, 실제 차량 제어 시스템을 구성하는 핵심 요소인 **센서 입력, MCU 판단, 모터 구동, 상태 출력, 무선 사용자 개입**의 관계를 하나의 프로토타입에서 확인하는 데 의의가 있습니다.

또한 부트캠프의 로봇팔과 컨베이어 실습에서 익힌 서보 제어, 센서 판별, EEPROM 저장, Bluetooth 통신 경험을 하나의 이동형 임베디드 시스템으로 통합했다는 점에서 학습 과정과 결과를 함께 보여줍니다.

## 참고

본 프로젝트는 교육 및 실습용 프로토타입이며 실제 도로 주행이나 안전이 보장되어야 하는 환경에서 사용할 수 없습니다.
