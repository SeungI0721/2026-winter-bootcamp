/* ===== 라이브러리 ===== */
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

/* ===== 핀 번호 설정 (L298P 쉴드 고정) ===== */
#define PIN_MOTOR_L_DIR     12
#define PIN_MOTOR_L_PWM     10
#define PIN_MOTOR_R_DIR     13
#define PIN_MOTOR_R_PWM     11

#define PIN_LED             5
#define PIN_BUZZER          4

/* ===== 초음파 센서 핀 설정 ===== */
#define PIN_US_TRIG         7
#define PIN_US_ECHO         8

/* ===== 적외선 센서(좌/우) ===== */
#define PIN_IR_LEFT         A1
#define PIN_IR_RIGHT        A2

#define IR_THRESHOLD        500   // 환경 따라 튜닝(0~1023)
#define IR_DETECTED(v)      ((v) < IR_THRESHOLD)  // 값이 작아질 때 감지라면 이대로
// 값이 커질 때 감지면: #define IR_DETECTED(v) ((v) > IR_THRESHOLD)

/* ===== 주행 및 회피 설정 ===== */
#define DRIVE_SPEED         120
#define DETECT_DISTANCE_CM  15
#define EMERGENCY_CM        6

#define EMERGENCY_LOCK_MS   2000UL
#define REVERSE_MS          800UL
#define REVERSE_SPEED       100
#define RECOVER_PAUSE_MS    100UL
#define CLEAR_DISTANCE_CM   12

#define AVOID_PAUSE_MS      100UL
#define AVOID_REVERSE_MS    300UL
#define AVOID_FORWARD_MS    500UL
#define AVOID_OUTER_PWM     160
#define AVOID_INNER_PWM     70
#define AVOID_SPIN_PWM      140

#define CMD_BUFFER_SIZE     20

/* ===== 주기(성능/반응성 개선) ===== */
#define RAINBOW_INTERVAL_MS 30UL    // 레인보우 업데이트 주기
#define SONAR_INTERVAL_MS   70UL    // 초음파 측정 주기

/* ===== 블루투스 설정 ===== */
SoftwareSerial bluetooth(2, 3); // RX, TX

/* ===== NeoPixel ===== */
#define NUM_PIXELS          3
Adafruit_NeoPixel led(NUM_PIXELS, PIN_LED, NEO_GRB + NEO_KHZ800);

/* ===== [추가] 조종 모드 ===== */
enum ControlMode {
  MODE_IDLE,     // 정지(명령 대기)
  MODE_AUTO,     // 기존 자율주행
  MODE_MANUAL    // [추가] 블루투스 수동조종
};
ControlMode mode = MODE_IDLE;

/* ===== [추가] 수동조종 상태 ===== */
int manualSpeed = 160;          // 0~255 (V 명령으로 변경 가능)
int manualTurnDelta = 90;       // 좌/우 회전 시 감속폭 (원하면 튜닝)
int manualLeftPWM = 0;          // 현재 적용할 좌측 PWM(-255~255)
int manualRightPWM = 0;         // 현재 적용할 우측 PWM(-255~255)
unsigned long lastManualCmdAt = 0;
#define MANUAL_TIMEOUT_MS 600UL  // 수동 모드에서 명령 끊기면 자동 정지

/* ===== 전역 변수 ===== */
char cmdBuffer[CMD_BUFFER_SIZE];
int cmdIndex = 0;

uint16_t rainbowHue = 0;
int avoidDir = -1; // -1: 좌회전, +1: 우회전

/* 초음파 거리 캐시 */
long lastDistCm = 999;
unsigned long nextSonarAt = 0;

/* 레인보우 타이밍 */
unsigned long nextRainbowAt = 0;

/* ===== 동작 상태 ===== */
enum RunState {
  STATE_NORMAL,
  STATE_EMERGENCY_LOCK,
  STATE_RECOVERY_REVERSE,
  STATE_RECOVER_PAUSE,     // (delay 제거용) 후진 후 잠깐 멈춤
  STATE_AVOID_PAUSE,
  STATE_AVOID_REVERSE,
  STATE_AVOID_FORWARD
};
RunState runState = STATE_NORMAL;
unsigned long stateUntil = 0;

/* ==============================
 * 부저(타이머 충돌 회피)
 * ============================== */
void playToneNoTimer(int pin, int frequency, int duration) {
  long period = 1000000L / frequency;
  long pulse = period / 2;
  for (long i = 0; i < duration * 1000L; i += period) {
    digitalWrite(pin, HIGH); delayMicroseconds(pulse);
    digitalWrite(pin, LOW);  delayMicroseconds(pulse);
  }
}

/* ==============================
 * NeoPixel 유틸
 * ============================== */
void setAllPixels(uint8_t R, uint8_t G, uint8_t B) {
  for (int i = 0; i < NUM_PIXELS; i++) led.setPixelColor(i, led.Color(R, G, B));
  led.show();
}

/* [수정] 호출 1번당 1스텝만 레인보우 진행(응답성 개선) */
void showIdleRainbowStep() {
  for (int i = 0; i < NUM_PIXELS; i++) {
    int pixelHue = rainbowHue + (i * 65536L / NUM_PIXELS);
    led.setPixelColor(i, led.gamma32(led.ColorHSV(pixelHue)));
  }
  led.show();
  rainbowHue += 512; // 속도(원하면 256/1024 등으로 조절)
}

/* ==============================
 * 모터 제어
 * ============================== */
void motorStop() {
  analogWrite(PIN_MOTOR_L_PWM, 0);
  analogWrite(PIN_MOTOR_R_PWM, 0);
}

void motorSetLeft(int pwm) {
  digitalWrite(PIN_MOTOR_L_DIR, pwm >= 0 ? HIGH : LOW);
  analogWrite(PIN_MOTOR_L_PWM, abs(pwm));
}

void motorSetRight(int pwm) {
  digitalWrite(PIN_MOTOR_R_DIR, pwm >= 0 ? HIGH : LOW);
  analogWrite(PIN_MOTOR_R_PWM, abs(pwm));
}

void driveForward(int pwm) { motorSetLeft(pwm); motorSetRight(pwm); }
void driveReverse(int pwm) { motorSetLeft(-pwm); motorSetRight(-pwm); }

/* [추가] 수동 PWM 적용 */
void applyManualDrive() {
  motorSetLeft(manualLeftPWM);
  motorSetRight(manualRightPWM);
}

/* ==============================
 * 초음파 거리 측정
 * ============================== */
long readDistanceCm() {
  digitalWrite(PIN_US_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);

  unsigned long dur = pulseIn(PIN_US_ECHO, HIGH, 18000UL); // ~3m 내외
  return dur == 0 ? 999 : (long)(dur / 58);
}

void updateSonarIfDue(unsigned long nowMs) {
  if ((long)(nowMs - nextSonarAt) < 0) return;
  lastDistCm = readDistanceCm();
  nextSonarAt = nowMs + SONAR_INTERVAL_MS;
}

/* ==============================
 * 상태 관리
 * ============================== */
void setState(RunState s) {
  runState = s;
  switch (s) {
    case STATE_NORMAL:
      break;

    case STATE_EMERGENCY_LOCK:
      motorStop();
      setAllPixels(255, 0, 0); // 빨간색
      break;

    case STATE_RECOVERY_REVERSE:
      setAllPixels(255, 0, 0);
      driveReverse(REVERSE_SPEED);
      break;

    case STATE_RECOVER_PAUSE:
      motorStop();
      setAllPixels(255, 0, 0);
      break;

    case STATE_AVOID_PAUSE:
      motorStop();
      setAllPixels(255, 255, 0); // 주황색
      break;

    case STATE_AVOID_REVERSE:
      setAllPixels(255, 255, 0);
      driveReverse(REVERSE_SPEED);
      break;

    case STATE_AVOID_FORWARD:
      if (avoidDir < 0) {                // 좌회전
        motorSetLeft(-AVOID_SPIN_PWM);   // 왼쪽 후진
        motorSetRight( AVOID_SPIN_PWM);  // 오른쪽 전진
      } else {                           // 우회전
        motorSetLeft( AVOID_SPIN_PWM);   // 왼쪽 전진
        motorSetRight(-AVOID_SPIN_PWM);  // 오른쪽 후진
      }
      break;
  }
}

void enterEmergencyLock(unsigned long nowMs) {
  setState(STATE_EMERGENCY_LOCK);
  playToneNoTimer(PIN_BUZZER, 440, 70);
  stateUntil = nowMs + EMERGENCY_LOCK_MS;
}

void startRecoveryReverse(unsigned long nowMs, unsigned long durationMs) {
  setState(STATE_RECOVERY_REVERSE);
  stateUntil = nowMs + durationMs;
}

void startRecoverPause(unsigned long nowMs) {
  setState(STATE_RECOVER_PAUSE);
  stateUntil = nowMs + RECOVER_PAUSE_MS;
}

void recoverDecision(unsigned long nowMs) {
  // 잠깐 멈춘 뒤 거리 다시 보고 반복/복귀
  updateSonarIfDue(nowMs); // 여기선 즉시 갱신해도 OK
  if (lastDistCm <= CLEAR_DISTANCE_CM) startRecoveryReverse(nowMs, 400UL);
  else setState(STATE_NORMAL);
}

void startAvoidWithDir(unsigned long nowMs, int dir) {
  avoidDir = (dir < 0) ? -1 : 1;
  setState(STATE_AVOID_PAUSE);
  stateUntil = nowMs + AVOID_PAUSE_MS;
}

void startAvoidRandom(unsigned long nowMs) {
  avoidDir = (random(0, 2) == 0) ? -1 : 1;
  setState(STATE_AVOID_PAUSE);
  stateUntil = nowMs + AVOID_PAUSE_MS;
}

/* ==============================
 * [추가] 수동 명령 처리
 * ============================== */
void manualSetStop() {
  manualLeftPWM = 0;
  manualRightPWM = 0;
}

void manualSetForward() {
  manualLeftPWM  = manualSpeed;
  manualRightPWM = manualSpeed;
}

void manualSetBackward() {
  manualLeftPWM  = -manualSpeed;
  manualRightPWM = -manualSpeed;
}

void manualSetLeft() {
  manualLeftPWM  = -manualSpeed; // 왼쪽 후진
  manualRightPWM =  manualSpeed; // 오른쪽 전진
}

void manualSetRight() {
  manualLeftPWM  =  manualSpeed; // 왼쪽 전진
  manualRightPWM = -manualSpeed; // 오른쪽 후진
}

/* ==============================
 * 명령 처리
 * ============================== */
void processCommand() {
  // 공통: 공백 제거(간단)

  /* ===== 기존: stop/start ===== */
  if (strcmp(cmdBuffer, "stop") == 0) {
    mode = MODE_IDLE;
    setState(STATE_NORMAL);
    motorStop();
    setAllPixels(0, 0, 0);
    manualSetStop();

  } else if (strcmp(cmdBuffer, "start") == 0) {
    mode = MODE_AUTO;
    setState(STATE_NORMAL);
    nextSonarAt = 0;
    nextRainbowAt = 0;

  /* ===== [추가] 수동 모드 진입/종료 ===== */
  } else if (strcmp(cmdBuffer, "manual") == 0 || strcmp(cmdBuffer, "mstart") == 0) {
    mode = MODE_MANUAL;
    setState(STATE_NORMAL);
    manualSetStop();
    lastManualCmdAt = millis();
    // 수동 모드 표시(파랑)
    setAllPixels(0, 0, 255);

  } else if (strcmp(cmdBuffer, "mstop") == 0) {
    mode = MODE_IDLE;
    setState(STATE_NORMAL);
    motorStop();
    setAllPixels(0, 0, 0);
    manualSetStop();

  /* ===== [추가] 수동 조종 1글자 명령(F/B/L/R/S) ===== */
  } else if (strlen(cmdBuffer) == 1) {
    char c = cmdBuffer[0];

  if (mode != MODE_MANUAL) { 
  mode = MODE_MANUAL; 
  setAllPixels(0,0,255);
  manualSetStop();
  setAllPixels(0, 0, 255); }

    if (mode == MODE_MANUAL) {
      if      (c == 'F') manualSetForward();
      else if (c == 'B') manualSetBackward();
      else if (c == 'L') manualSetLeft();
      else if (c == 'R') manualSetRight();
      else if (c == 'S') manualSetStop();
      lastManualCmdAt = millis();
    }

  /* ===== [추가] 속도 명령: V0~V9 또는 V120 ===== */
  } else if (cmdBuffer[0] == 'V' || cmdBuffer[0] == 'v') {
    // 예: V0~V9  /  V120
    int val = atoi(cmdBuffer + 1);
    if (val >= 0 && val <= 9) {
      manualSpeed = map(val, 0, 9, 0, 255);
    } else if (val >= 0 && val <= 255) {
      manualSpeed = val;
    }
    // 수동 모드에서 속도 변경 즉시 반영하고 싶으면:
    if (mode == MODE_MANUAL) lastManualCmdAt = millis();
  }
}

void handleCommandChar(char ch) {
  if (ch == '\r') return;

  if (ch == '_' || ch == '\n') {
    cmdBuffer[cmdIndex] = '\0';
    processCommand();
    cmdIndex = 0;
  } else if (cmdIndex < CMD_BUFFER_SIZE - 1) {
    cmdBuffer[cmdIndex++] = ch;
  }
}

void checkSerialCommand() {
  while (Serial.available() > 0) handleCommandChar((char)Serial.read());
  while (bluetooth.available() > 0) handleCommandChar((char)bluetooth.read());
}

/* ==============================
 * 초기화 및 메인 루프
 * ============================== */
void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);

  pinMode(PIN_MOTOR_L_DIR, OUTPUT); pinMode(PIN_MOTOR_L_PWM, OUTPUT);
  pinMode(PIN_MOTOR_R_DIR, OUTPUT); pinMode(PIN_MOTOR_R_PWM, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_US_TRIG, OUTPUT); pinMode(PIN_US_ECHO, INPUT);

  pinMode(PIN_IR_LEFT, INPUT);
  pinMode(PIN_IR_RIGHT, INPUT);

  randomSeed(analogRead(A0));

  led.begin();
  led.setBrightness(200);
  led.show();

  motorStop();

  // 부팅음(짧게)
  playToneNoTimer(PIN_BUZZER, 659, 80);
  playToneNoTimer(PIN_BUZZER, 784, 80);
}

void loop() {
  checkSerialCommand();

  unsigned long nowMs = millis();

  /* ===== 안전을 위해 초음파는 모드 상관없이 갱신 ===== */
  updateSonarIfDue(nowMs);

  /* ===== 상태 진행(긴급/회피 시퀀스) ===== */
  if (runState != STATE_NORMAL) {
    if (nowMs >= stateUntil) {
      switch (runState) {
        case STATE_EMERGENCY_LOCK:
          startRecoveryReverse(nowMs, REVERSE_MS);
          break;

        case STATE_RECOVERY_REVERSE:
          startRecoverPause(nowMs);
          break;

        case STATE_RECOVER_PAUSE:
          recoverDecision(nowMs);
          break;

        case STATE_AVOID_PAUSE:
          setState(STATE_AVOID_REVERSE);
          stateUntil = nowMs + AVOID_REVERSE_MS;
          break;

        case STATE_AVOID_REVERSE:
          setState(STATE_AVOID_FORWARD);
          stateUntil = nowMs + AVOID_FORWARD_MS;
          break;

        case STATE_AVOID_FORWARD:
          setState(STATE_NORMAL);
          break;

        default:
          setState(STATE_NORMAL);
          break;
      }
    }
    return; // 시퀀스 중에는 사용자 조종/자율 제어 막음
  }

  /* ===== 공통 긴급 조건(모든 모드 우선) ===== */
  if (lastDistCm <= EMERGENCY_CM) {
    enterEmergencyLock(nowMs);
    return;
  }

  /* ===== IDLE 모드 ===== */
  if (mode == MODE_IDLE) {
    motorStop();
    // IDLE 표시 레인보우(선택)
    if ((long)(nowMs - nextRainbowAt) >= 0) {
      showIdleRainbowStep();
      nextRainbowAt = nowMs + RAINBOW_INTERVAL_MS;
    }
    return;
  }

  /* ===== MANUAL 모드 ===== */
  if (mode == MODE_MANUAL) {
    // 명령 끊기면 정지(안전)
    if (nowMs - lastManualCmdAt > MANUAL_TIMEOUT_MS) {
      manualSetStop();
    }

    // 수동 모드 LED 표시(파랑 유지하고 싶으면 여기서 유지)
    // setAllPixels(0, 0, 255); // 너무 자주 show()하면 부담이라 보통은 진입 시 1회만

    applyManualDrive();

    // (선택) 수동 모드에서도 가까우면 경고 회피를 넣고 싶다면 여기서 DETECT_DISTANCE_CM 처리 가능
    // if (lastDistCm <= DETECT_DISTANCE_CM) startAvoidRandom(nowMs);

    return;
  }

  /* ===== AUTO 모드(기존 로직) ===== */
  if (mode == MODE_AUTO) {
    /* ===== NORMAL: IR 좌/우 회피 (초음파보다 우선) ===== */
    int irL = analogRead(PIN_IR_LEFT);
    int irR = analogRead(PIN_IR_RIGHT);

    bool hitL = IR_DETECTED(irL);
    bool hitR = IR_DETECTED(irR);

    if (hitL && !hitR) {        // 왼쪽 감지 -> 오른쪽으로 회피
      startAvoidWithDir(nowMs, +1);
      return;
    } else if (!hitL && hitR) { // 오른쪽 감지 -> 왼쪽으로 회피
      startAvoidWithDir(nowMs, -1);
      return;
    } else if (hitL && hitR) {  // 양쪽 감지 -> 랜덤 회피
      startAvoidRandom(nowMs);
      return;
    }

    /* ===== LED 레인보우(주기 제한) ===== */
    if ((long)(nowMs - nextRainbowAt) >= 0) {
      showIdleRainbowStep();
      nextRainbowAt = nowMs + RAINBOW_INTERVAL_MS;
    }

    /* ===== 전진 ===== */
    driveForward(DRIVE_SPEED);

    /* ===== 초음파 기반 회피 ===== */
    if (lastDistCm <= DETECT_DISTANCE_CM) {
      startAvoidRandom(nowMs);
    }
  }
}
