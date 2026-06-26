/**
 * 스마트 팩토리 통합 컨베이어 시스템 (단순화 버전)
 * 
 * 동작 순서:
 * 1. 컨베이어 벨트 가동
 * 2. 적외선 센서로 제품 감지 → 일시 정지
 * 3. 컬러 센서까지 이동
 * 4. 색상 분석 (빨강/초록/파랑)
 * 5. 서보 모터로 분류 방향 설정
 * 6. RGB LED로 색상 표시
 * 7. 컨베이어 벨트 재가동 → 제품 분류
 
/* ===== 라이브러리 ===== */
#include <Adafruit_NeoPixel.h>
#include <Adafruit_TCS34725.h>
#include <Servo.h>
#include <Wire.h>
#include <SoftwareSerial.h>

/* ===== 핀 번호 설정 ===== */
#define PIN_MOTOR_DIR       13    // DC 모터 방향
#define PIN_MOTOR_SPEED     11    // DC 모터 속도 (PWM)
#define PIN_SERVO           9     // 서보 모터
#define PIN_LED             5     // RGB LED
#define PIN_IR_SENSOR       A0    // 적외선 센서
#define PIN_BUZZER          4     // 부저
#define PIN_DIO             7     // 다이오드

/* ===== 서보 각도 설정 ===== */
#define ANGLE_RED           60    // 빨간색 제품
#define ANGLE_GREEN         40    // 초록색 제품
#define ANGLE_BLUE          0     // 파란색 제품
#define ANGLE_YE            90   // 노란색 

/* ===== LED 설정 ===== */
#define NUM_PIXELS          3     // LED 개수
#define LED_BRIGHTNESS      255   // LED 밝기

/* ===== 색상 센서 설정 ===== */
#define RAW_MAX             21504 // 센서 최대값
#define MAPPED_MAX          1000  // 매핑 최대값
#define MIN_SUM             15    // 유효 색상 최소값

/* ===== 모터 설정 ===== */
#define MOTOR_SPEED         100   // 컨베이어 속도
#define MOTOR_DIR_FORWARD   HIGH  // 전진 방향

/* ===== 타이밍 설정 (ms) ===== */
#define DELAY_IR_DETECT     2000  // 적외선 감지 후 대기
#define DELAY_COLOR_DETECT  1500  // 색상 분석 후 대기
#define DELAY_NEXT_PRODUCT  1000  // 다음 제품 대기

/* ===== 명령 버퍼 설정 ===== */
#define CMD_BUFFER_SIZE     20    // 명령 버퍼 크기

/*블루투스*/
#define BT_TXD 2
#define BT_RXD 3
SoftwareSerial bluetooth(BT_RXD, BT_TXD);

/* ===== 전역 객체 ===== */
Adafruit_TCS34725 colorSensor = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
Servo servo;
Adafruit_NeoPixel led = Adafruit_NeoPixel(NUM_PIXELS, PIN_LED, NEO_GRB + NEO_KHZ800);

/* ===== 전역 변수 ===== */
uint16_t rawR, rawG, rawB, rawC;
int r, g, b;
int productCount = 0;
bool autoMode = false;  // 자동화 모드 (기본: 중지)
char cmdBuffer[CMD_BUFFER_SIZE];
int cmdIndex = 0;

uint8_t rr = 255, gg = 0, bb = 0;
uint8_t phase = 0;
const uint8_t STEP = 5;

/**
 * 초기화
 */
void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);
  
  // 핀 모드 설정
  pinMode(PIN_MOTOR_DIR, OUTPUT);
  pinMode(PIN_MOTOR_SPEED, OUTPUT);
  pinMode(PIN_IR_SENSOR, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_DIO, OUTPUT);
  
  // 모터 초기화
  digitalWrite(PIN_MOTOR_DIR, MOTOR_DIR_FORWARD);
  analogWrite(PIN_MOTOR_SPEED, 0);
  digitalWrite(PIN_DIO, LOW);
  
  // 서보 초기화
  servo.attach(PIN_SERVO);
  servo.write(ANGLE_RED);  // 기본 위치
  delay(500);
  servo.write(ANGLE_GREEN);  // 기본 위치
  delay(500);
  servo.write(ANGLE_BLUE);  // 기본 위치
  servo.detach();
  
  // 컬러 센서 초기화
  if (!colorSensor.begin()) {
    Serial.println("[오류] 컬러 센서를 찾을 수 없습니다!");
    while (1) delay(1000);
  }
  
  // LED 초기화
  led.begin();
  led.setBrightness(LED_BRIGHTNESS);
  led.show();
  
  // 시작 메시지
  Serial.println("\n========================================");
  Serial.println("  스마트 팩토리 컨베이어 시스템");
  Serial.println("========================================");
  Serial.println("초기화 완료");
  Serial.println("\n명령어:");
  Serial.println("  start_ : 자동화 시작");
  Serial.println("  stop_  : 자동화 중지");
  Serial.println("========================================");
  Serial.println("명령 대기중...\n");
  
  // 시작 알림음
  tone(PIN_BUZZER, 523, 120); delay(150);
  tone(PIN_BUZZER, 659, 120); delay(150);
  tone(PIN_BUZZER, 784, 160); delay(200);
  noTone(PIN_BUZZER);
  delay(100);
}

void beepN(int freq, byte n, int onMs = 90, int gapMs = 80) { // ++비프 함수++
  for (byte i = 0; i < n; i++) {
    tone(PIN_BUZZER, freq, onMs);
    delay(onMs + 5);
    noTone(PIN_BUZZER);
    delay(gapMs);
  }
}

void rainbowStepRGB() { //색상 함수
  switch (phase) {
    case 0: gg = (gg + STEP >= 255) ? 255 : gg + STEP; if (gg == 255) phase = 1; break; // R=255, G↑
    case 1: rr = (rr >= STEP) ? rr - STEP : 0;        if (rr == 0)   phase = 2; break; // G=255, R↓
    case 2: bb = (bb + STEP >= 255) ? 255 : bb + STEP; if (bb == 255) phase = 3; break; // G=255, B↑
    case 3: gg = (gg >= STEP) ? gg - STEP : 0;        if (gg == 0)   phase = 4; break; // B=255, G↓
    case 4: rr = (rr + STEP >= 255) ? 255 : rr + STEP; if (rr == 255) phase = 5; break; // B=255, R↑
    case 5: bb = (bb >= STEP) ? bb - STEP : 0;        if (bb == 0)   phase = 0; break; // R=255, B↓
  }
}

void showIdleRainbow() {
  rainbowStepRGB();
  for (int i = 0; i < NUM_PIXELS; i++) {
    led.setPixelColor(i, led.Color(rr, gg, bb));
  }
  led.show();
  delay(20); // 너무 빠르게 루프가 도는 것 방지 + 눈에 보이는 속도
}

/**
 * 메인 루프
 */
void loop() {
  // Serial 명령 처리
  checkSerialCommand();
  
  // 자동화 모드가 아니면 대기
  if (!autoMode) {
    showIdleRainbow();
    return;
  }

  // 1. 제품 감지 확인 (HIGH = 감지됨)
  if (digitalRead(PIN_IR_SENSOR) == HIGH) {
    digitalWrite(PIN_DIO, LOW);  // 제품 없음 → LED OFF

    showIdleRainbow();
    return;
  }
  
  // 2. 제품 감지됨!
  productCount++;
  Serial.println("========================================");
  Serial.print("제품 #");
  Serial.print(productCount);
  Serial.println(" 감지됨!");
  digitalWrite(PIN_DIO, HIGH);
  Serial.println("========================================");
  
  // 3. 컨베이어 일시 정지
  analogWrite(PIN_MOTOR_SPEED, 0);
  delay(100);
  // DELAY_IR_DETECT 동안 Serial 체크
  if (!delayWithSerialCheck(DELAY_IR_DETECT)) return;
  
  // 4. 컬러 센서까지 이동
  analogWrite(PIN_MOTOR_SPEED, MOTOR_SPEED);
  
  int sum = 0;
  int attempts = 0;
  do {
    // Serial 명령 체크 (stop 명령 즉시 반응)
    checkSerialCommand();
    if (!autoMode) {
      analogWrite(PIN_MOTOR_SPEED, 0);
      return;
    }
    
    colorSensor.getRawData(&rawR, &rawG, &rawB, &rawC);
    r = map(rawR, 0, RAW_MAX, 0, MAPPED_MAX);
    g = map(rawG, 0, RAW_MAX, 0, MAPPED_MAX);
    b = map(rawB, 0, RAW_MAX, 0, MAPPED_MAX);
    sum = r + g + b;
    
    attempts++;
    if (attempts > 1000) break;  // 타임아웃 방지
    delay(10);
  } while (sum < MIN_SUM);
  
  // 5. 컨베이어 정지 (색상 분석)
  analogWrite(PIN_MOTOR_SPEED, 0);
  delay(100);
  
  // 6. 색상 정보 출력
  Serial.println("--- 색상 분석 결과 ---");
  Serial.print("Raw -> R: ");
  Serial.print(rawR);
  Serial.print(", G: ");
  Serial.print(rawG);
  Serial.print(", B: ");
  Serial.println(rawB);
  
  Serial.print("RGB -> R: ");
  Serial.print(r);
  Serial.print(", G: ");
  Serial.print(g);
  Serial.print(", B: ");
  Serial.println(b);
  
  // 7. 색상 판별 및 분류
  int ledR = 0, ledG = 0, ledB = 0;
  int servoAngle = ANGLE_BLUE;
  const char* colorName = "알 수 없음";

  int frequency = 0;
  byte beepCount = 1;
  
  if (r > g && r > b) {
    // 빨간색 = 도
    colorName = "빨간색";
    servoAngle = ANGLE_RED;
    ledR = 255; ledG = 0; ledB = 0;
    frequency = 523;
    beepCount = 1;
  } 
  else if (r > b && g > b && abs(r - g) < 120) {
    // 노란색 = 라
    colorName = "노란색";
    servoAngle = ANGLE_YE;
    ledR = 255; ledG = 255; ledB = 0;
    frequency = 880;
    beepCount = 2;
  } 
  else if (g > r && g > b) {
    // 초록색 = 솔
    colorName = "초록색";
    servoAngle = ANGLE_GREEN;
    ledR = 0; ledG = 255; ledB = 0;
    frequency = 784;
    beepCount = 3;
  } 
  else {
    // 파란색 = 미
    colorName = "파란색";
    servoAngle = ANGLE_BLUE;
    ledR = 0; ledG = 0; ledB = 255;
    frequency = 659;
    beepCount = 4;
  }
  
  Serial.print("판별 색상: ");
  Serial.println(colorName);
  Serial.println("---------------------");

  // 8. 서보 모터로 분류 방향 설정
  servo.attach(PIN_SERVO);
  servo.write(servoAngle);
  if (!delayWithSerialCheck(500)) {
    servo.detach();
    return;
  }
  
  // 9. RGB LED 색상 표시
  for (int i = 0; i < NUM_PIXELS; i++) {
    led.setPixelColor(i, led.Color(ledR, ledG, ledB));
  }
  led.show();

  beepN(frequency, beepCount);

  if (!delayWithSerialCheck(DELAY_COLOR_DETECT)) {
    servo.detach();
    return;
  }

  // 10. 서보 분리 및 컨베이어 재가동
  servo.detach();
  analogWrite(PIN_MOTOR_SPEED, MOTOR_SPEED);
  if (!delayWithSerialCheck(DELAY_NEXT_PRODUCT)) return;
  
  digitalWrite(PIN_DIO, LOW);  // 처리 끝 → LED OFF
  Serial.println("제품 처리 완료\n");
}

/**
 * delay 동안 Serial 명령 체크
 * @param ms 대기 시간 (밀리초)
 * @return autoMode가 계속 true면 true, false로 변경되면 false
 */
bool delayWithSerialCheck(unsigned long ms) {
  unsigned long startTime = millis();
  while (millis() - startTime < ms) {
    checkSerialCommand();
    if (!autoMode) {
      return false;  // stop 명령 받음
    }
    delay(50);  // 50ms 간격으로 체크
  }
  return true;  // 정상 완료
}

/**
 * Serial 명령 확인 및 처리
 */
void checkSerialCommand() {
  // Serial 데이터가 없으면 리턴
  // ✅ Serial 먼저 처리
  while (Serial.available() > 0) {
    char ch = Serial.read();
    handleCommandChar(ch);
    }

  // ✅ Bluetooth도 처리
  while (bluetooth.available() > 0) {
    char ch = bluetooth.read();
    handleCommandChar(ch);
    }
  }
  
  // 문자 읽기
  char ch = Serial.read();
  
  void handleCommandChar(char ch) {
  // 개행 문자 무시
  if (ch == '\n' || ch == '\r') return;

  // '_' 문자를 만나면 명령 처리
  if (ch == '_') {
    cmdBuffer[cmdIndex] = '\0';  // 문자열 종료
    processCommand();
    cmdIndex = 0;  // 버퍼 초기화
    return;
  }

  // 버퍼에 문자 추가
  if (cmdIndex < CMD_BUFFER_SIZE - 1) {
    cmdBuffer[cmdIndex] = ch;
    cmdIndex++;
  }
}

/**
 * 명령 처리
 */
void processCommand() {
  Serial.print("[명령 수신] ");
  Serial.println(cmdBuffer);
  
  // start 명령
  if (strcmp(cmdBuffer, "start") == 0) {
    if (autoMode) {
      Serial.println("[알림] 이미 자동화 모드가 실행 중입니다.");
      return;
    }
    
    autoMode = true;
    analogWrite(PIN_MOTOR_SPEED, MOTOR_SPEED);
    
    Serial.println("========================================");
    Serial.println("  자동화 시작");
    Serial.println("========================================");
    Serial.println("컨베이어 가동 시작\n");
    
    tone(PIN_BUZZER, 523, 100);
    delay(150);
    tone(PIN_BUZZER, 659, 100);
    delay(150);
    return;
  }
  
  // stop 명령
  if (strcmp(cmdBuffer, "stop") == 0) {
    if (!autoMode) {
      Serial.println("[알림] 이미 자동화 모드가 중지되어 있습니다.");
      return;
    }
    
    autoMode = false;
    analogWrite(PIN_MOTOR_SPEED, 0);
    
    // LED 끄기
    for (int i = 0; i < NUM_PIXELS; i++) {
      led.setPixelColor(i, led.Color(0, 0, 0));
    }
    led.show();
    
    Serial.println("========================================");
    Serial.println("  자동화 중지");
    Serial.println("========================================");
    Serial.println("컨베이어 정지\n");
    Serial.println("명령 대기중...\n");

    tone(PIN_BUZZER, 659, 100);
    delay(150);
    tone(PIN_BUZZER, 523, 100);
    delay(150);
    return;
  }
  
  // 알 수 없는 명령
  Serial.print("[오류] 알 수 없는 명령: ");
  Serial.println(cmdBuffer);
  Serial.println("사용 가능: start_, stop_\n");
}
