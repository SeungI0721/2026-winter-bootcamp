/**
 * 로봇 팔 - Bluetooth EEPROM 자동화 시스템 (안정성 강화 버전)
 * 적용:
 *  (2) 각도 범위 재조정(특히 그립 스톨 방지)
 *  (3) 재생 시 축 순차 이동 + 축별 속도/안정화 딜레이
 */

#include <Servo.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

/* ===== 디버그 모드 설정 ===== */
#define ENABLE_DEBUG      0
#define ENABLE_BLUETOOTH  1

/* ===== Bluetooth 설정 ===== */
#define PIN_BT_TX   3
#define PIN_BT_RX   2

#if ENABLE_BLUETOOTH
SoftwareSerial BTSerial(PIN_BT_RX, PIN_BT_TX);
#endif

/* ===== 서보 설정 ===== */
Servo servo[4];
int pin[4] = {4, 5, 6, 7};

// 각도 범위 설정
// ※ 그립(3번)은 스톨(끝까지 닫힘) 방지를 위해 max를 보수적으로 낮춤
//    실제 기구에 맞게 maxAngles[3] 값을 70~95 사이에서 튜닝 추천
int minAngles[4] = {0,  50, 50, 15};
int maxAngles[4] = {180,180,140, 85};   // <-- (2) 그립 상한 낮춤 (기본 85)

// 초기 각도
int angles[4]     = {90, 80, 90, 50};   // 현재 각도
int prevAngles[4] = {90, 80, 90, 50};   // (선택) 추적용

/* ===== EEPROM 설정 ===== */
#define EEPROM_ADDR_COUNT   0
#define EEPROM_ADDR_START   4
#define MAX_POSITIONS       8

/* ===== 전역 변수 ===== */
int savedCount = 0;
bool isPlaying = false;
bool autoRepeat = false;
bool playOnce = false;
int repeatCount = 0;

char cmdBuffer[20];
int cmdIndex = 0;
unsigned long lastCharTime = 0;
#define CMD_TIMEOUT 100

/* ===== 재생(순차 이동) 튜닝 파라미터 ===== */
#define STEP_DELAY_BASE_MS     15
#define STEP_DELAY_ELBOW_MS    25   // 하중 큰 축 느리게
#define STEP_DELAY_WRIST_MS    20
#define STEP_DELAY_GRIP_MS     30   // 그립 가장 느리게
#define STABILIZE_BEFORE_GRIP  180  // 팔 위치 안정화 후 그립 (ms)

static inline int clampAngle(int idx, int a) {
  if (a < minAngles[idx]) return minAngles[idx];
  if (a > maxAngles[idx]) return maxAngles[idx];
  return a;
}

void setup() {
#if ENABLE_DEBUG
  Serial.begin(9600);
  delay(50);
  Serial.println("v1.2-stable");
#endif

  // 서보 attach 후 "현재 추정" 없이 안전 초기 각도로 천천히 접근 (불필요한 큰 이동 방지)
  for (int i = 0; i < 4; i++) {
    angles[i] = clampAngle(i, angles[i]);
    servo[i].attach(pin[i]);
    servo[i].write(angles[i]);
    delay(250);
  }

  // EEPROM 데이터 읽기
  savedCount = EEPROM.read(EEPROM_ADDR_COUNT);
  if (savedCount > MAX_POSITIONS) {
    savedCount = 0;
    EEPROM.write(EEPROM_ADDR_COUNT, 0);
  }

#if ENABLE_DEBUG
  Serial.print("Data:");
  Serial.print(savedCount);
  Serial.print("/");
  Serial.println(MAX_POSITIONS);
#endif

#if ENABLE_BLUETOOTH
  BTSerial.begin(9600);
  delay(300);
#if ENABLE_DEBUG
  Serial.println("BT OK");
#endif
#endif
}

void loop() {
  handleCommand();

  if (playOnce && !isPlaying) {
    playOnce = false;
    autoRepeat = false;
    playAllPositions();
    return;
  }

  if (autoRepeat && !isPlaying) {
    playAllPositions();
    return;
  }

  if (!isPlaying) {
    handleJoystick();
  }
}

/**
 * 조이스틱 제어
 */
void handleJoystick() {
  int val[4];
  val[0] = analogRead(14);              // A0 - 베이스
  val[1] = analogRead(15);              // A1 - 팔꿈치
  val[2] = 1024 - analogRead(16);       // A2 - 손목 (반대)
  val[3] = 1024 - analogRead(17);       // A3 - 그립 (반대)

  for (int i = 0; i < 4; i++) {
    int before = angles[i];

    if (val[i] > 1000) {
      angles[i] = clampAngle(i, angles[i] + 1);
    } else if (val[i] < 100) {
      angles[i] = clampAngle(i, angles[i] - 1);
    }

    // 변할 때만 write
    if (angles[i] != before) {
      servo[i].write(angles[i]);
      prevAngles[i] = angles[i];
    }
  }

  delay(20);
}

/**
 * Bluetooth 명령 처리
 */
void handleCommand() {
  // 재생 중에는 stop만 처리
  if (isPlaying) {
    char ch = '\0';
#if ENABLE_BLUETOOTH
    if (BTSerial.available() > 0) ch = BTSerial.read();
#endif
    if (ch != '\0') {
      if (ch == '\n' || ch == '\r' || ch == '_') {
        if (cmdIndex > 0) {
          cmdBuffer[cmdIndex] = '\0';
          if (strcmp(cmdBuffer, "stop") == 0) stopAutoMode();
          cmdIndex = 0;
        }
      } else if (ch != ' ' && cmdIndex < 19) {
        cmdBuffer[cmdIndex++] = ch;
        lastCharTime = millis();
      }
    }
    return;
  }

  // 타임아웃 처리
  if (cmdIndex > 0 && (millis() - lastCharTime) > CMD_TIMEOUT) {
    cmdBuffer[cmdIndex] = '\0';
    processCommand();
    cmdIndex = 0;
    return;
  }

  char ch = '\0';
#if ENABLE_BLUETOOTH
  if (BTSerial.available() > 0) {
    ch = BTSerial.read();
    lastCharTime = millis();
  }
#endif
  if (ch == '\0') return;

  if (ch == '\n' || ch == '\r' || ch == '_') {
    if (cmdIndex > 0) {
      cmdBuffer[cmdIndex] = '\0';
#if ENABLE_DEBUG
      Serial.print("[");
      Serial.print(cmdBuffer);
      Serial.println("]");
#endif
      processCommand();
      cmdIndex = 0;
    }
    return;
  }

  if (ch == ' ') return;

  if (cmdIndex < 19) cmdBuffer[cmdIndex++] = ch;
}

void processCommand() {
  if (strcmp(cmdBuffer, "save") == 0) {
    saveCurrentPosition();
  } else if (strcmp(cmdBuffer, "play") == 0) {
#if ENABLE_BLUETOOTH
    while (BTSerial.available() > 0) BTSerial.read();
#endif
    cmdIndex = 0;
    cmdBuffer[0] = '\0';
    playOnce = true;
    autoRepeat = false;
    isPlaying = false;
  } else if (strcmp(cmdBuffer, "auto") == 0) {
    startAutoMode();
  } else if (strcmp(cmdBuffer, "stop") == 0) {
    stopAutoMode();
  } else if (strcmp(cmdBuffer, "clear") == 0) {
    clearAllPositions();
  } else if (strcmp(cmdBuffer, "list") == 0) {
    listAllPositions();
  }
#if ENABLE_DEBUG
  else {
    Serial.print("ERR:");
    Serial.println(cmdBuffer);
  }
#endif
}

/**
 * 현재 위치 저장
 */
void saveCurrentPosition() {
  if (savedCount >= MAX_POSITIONS) {
#if ENABLE_DEBUG
    Serial.println("Full");
#endif
    return;
  }

  // EEPROM 저장 (각도는 이미 clamp된 상태지만 혹시 몰라 저장 전 보정)
  int addr = EEPROM_ADDR_START + (savedCount * 4);
  for (int i = 0; i < 4; i++) {
    angles[i] = clampAngle(i, angles[i]);
    EEPROM.write(addr + i, angles[i]);
    delay(4);
  }

  savedCount++;
  EEPROM.write(EEPROM_ADDR_COUNT, savedCount);
  delay(4);

#if ENABLE_DEBUG
  Serial.print("Save#");
  Serial.println(savedCount);
#endif
}

void startAutoMode() {
  if (savedCount == 0) {
#if ENABLE_DEBUG
    Serial.println("NoData");
#endif
    return;
  }
#if ENABLE_BLUETOOTH
  while (BTSerial.available() > 0) BTSerial.read();
#endif
  cmdIndex = 0;
  cmdBuffer[0] = '\0';
  autoRepeat = true;
  playOnce = false;
  isPlaying = false;
  repeatCount = 0;

#if ENABLE_DEBUG
  Serial.print("Auto:");
  Serial.println(savedCount);
#endif
}

void stopAutoMode() {
  autoRepeat = false;
  playOnce = false;
  isPlaying = false;

#if ENABLE_BLUETOOTH
  while (BTSerial.available() > 0) BTSerial.read();
#endif
  cmdIndex = 0;
  cmdBuffer[0] = '\0';

#if ENABLE_DEBUG
  Serial.print("Stop:");
  Serial.println(repeatCount);
#endif
}

void playAllPositions() {
  if (savedCount == 0) {
    autoRepeat = false;
    return;
  }

  if (autoRepeat) repeatCount++;

  isPlaying = true;

  for (int pos = 0; pos < savedCount; pos++) {
    handleCommand();
    if (!autoRepeat && !isPlaying) return;

    int addr = EEPROM_ADDR_START + (pos * 4);
    int targetAngles[4];
    bool dataValid = true;

    for (int i = 0; i < 4; i++) {
      targetAngles[i] = EEPROM.read(addr + i);
      // EEPROM은 0~255라서 음수는 안 나오지만, 범위/기구 한계 보정
      if (targetAngles[i] > 180) { dataValid = false; break; }
      targetAngles[i] = clampAngle(i, targetAngles[i]);
    }

    if (!dataValid) continue;

#if ENABLE_DEBUG
    Serial.print("#");
    Serial.print(pos + 1);
    Serial.print(":");
    Serial.print(targetAngles[0]); Serial.print(",");
    Serial.print(targetAngles[1]); Serial.print(",");
    Serial.print(targetAngles[2]); Serial.print(",");
    Serial.println(targetAngles[3]);
#endif

    if (!moveToPosition(targetAngles)) return;

    // 대기(재생 동작 사이 휴식) - 전원 피크 완화
    for (int i = 0; i < 80; i++) {
      handleCommand();
      if (!autoRepeat && !isPlaying) return;
      delay(10);
    }
  }

  isPlaying = false;
}

/* ===== (3) 축 순차 이동 함수 ===== */
bool moveAxisTo(uint8_t axis, int target, int stepDelayMs) {
  target = clampAngle(axis, target);

  while (angles[axis] != target) {
    handleCommand();
    if (!autoRepeat && !isPlaying) return false;

    int before = angles[axis];
    if (angles[axis] < target) angles[axis]++;
    else angles[axis]--;

    if (angles[axis] != before) {
      servo[axis].write(angles[axis]);
      prevAngles[axis] = angles[axis];
    }
    delay(stepDelayMs);
  }
  return true;
}

/**
 * 목표 위치로 이동 (축 순차 이동)
 * 순서: 베이스 → 팔꿈치(하중) → 손목 → (안정화) → 그립(스톨 위험)
 */
bool moveToPosition(int target[]) {
  // 먼저 전체 target을 기구 안전 범위로 보정
  for (int i = 0; i < 4; i++) target[i] = clampAngle(i, target[i]);

  // 0: 베이스
  if (!moveAxisTo(0, target[0], STEP_DELAY_BASE_MS)) return false;

  // 1: 팔꿈치 (하중 큼)
  if (!moveAxisTo(1, target[1], STEP_DELAY_ELBOW_MS)) return false;

  // 2: 손목
  if (!moveAxisTo(2, target[2], STEP_DELAY_WRIST_MS)) return false;

  // 팔 위치 안정화 후 그립 (전류 피크 줄임)
  for (int t = 0; t < STABILIZE_BEFORE_GRIP / 10; t++) {
    handleCommand();
    if (!autoRepeat && !isPlaying) return false;
    delay(10);
  }

  // 3: 그립 (가장 느리게)
  if (!moveAxisTo(3, target[3], STEP_DELAY_GRIP_MS)) return false;

  return true;
}

void clearAllPositions() {
  autoRepeat = false;
  playOnce = false;
  isPlaying = false;
  repeatCount = 0;

  savedCount = 0;
  EEPROM.write(EEPROM_ADDR_COUNT, 0);
  delay(5);

#if ENABLE_DEBUG
  Serial.println("Clear");
#endif
}

void listAllPositions() {
#if ENABLE_DEBUG
  if (savedCount == 0) {
    Serial.println("Empty");
    return;
  }

  Serial.print("List:");
  Serial.println(savedCount);

  for (int pos = 0; pos < savedCount; pos++) {
    int addr = EEPROM_ADDR_START + (pos * 4);

    Serial.print("#");
    Serial.print(pos + 1);
    Serial.print(":");

    for (int i = 0; i < 4; i++) {
      Serial.print(EEPROM.read(addr + i));
      if (i < 3) Serial.print(",");
    }
    Serial.println();
  }
#endif
}
