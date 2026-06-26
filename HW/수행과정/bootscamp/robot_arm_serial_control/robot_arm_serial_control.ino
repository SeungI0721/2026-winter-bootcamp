/**

 * 로봇 팔 - 3단계: Serial 입력 + 서보 모터 제어
 * 
 * Serial 명령으로 각 서보 모터를 원격 제어
 * 명령 형식: arm0_90 (Enter로 전송)
 * arm0~3: 베이스, 팔꿈치, 손목, 그립
 */

#include <Servo.h>

/* ===== 서보 설정 ===== */
Servo servo[4];
int pin[4] = {4, 5, 6, 7};

// 각도 범위 설정 (쉽게 수정 가능)
int minAngles[4] = {0, 70, 60, 5};      // 최소 각도: 베이스, 팔꿈치, 손목, 그립
int maxAngles[4] = {130, 120, 120, 50}; // 최대 각도: 베이스, 팔꿈치, 손목, 그립

int angles[4] = {90, 80, 90, 30};  // 현재 각도: 베이스, 팔꿈치, 손목, 그립(열린 상태)

char cmdBuffer[20];
int cmdIndex = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Serial 원격 제어 시작\n");
  
  Serial.println("명령 형식: arm0_90 (Enter로 전송)");
  Serial.println("  arm0: 베이스");
  Serial.println("  arm1: 팔꿈치");
  Serial.println("  arm2: 손목");
  Serial.println("  arm3: 그립\n");
  
  // 각도 범위 출력
  Serial.println("각도 범위:");
  Serial.print("  베이스: ");
  Serial.print(minAngles[0]);
  Serial.print(" ~ ");
  Serial.println(maxAngles[0]);
  
  Serial.print("  팔꿈치: ");
  Serial.print(minAngles[1]);
  Serial.print(" ~ ");
  Serial.println(maxAngles[1]);
  
  Serial.print("  손목: ");
  Serial.print(minAngles[2]);
  Serial.print(" ~ ");
  Serial.println(maxAngles[2]);
  
  Serial.print("  그립: ");
  Serial.print(minAngles[3]);
  Serial.print(" ~ ");
  Serial.println(maxAngles[3]);
  
  Serial.println("\n초기 위치:");
  Serial.println("베이스:90 | 팔꿈치:90 | 손목:90 | 그립:5\n");
  
  // 서보 연결 및 초기화
  for (int i = 0; i < 4; i++) {
    servo[i].attach(pin[i]);
    servo[i].write(angles[i]);
  }
}

void loop() {
  // Serial 입력 처리
  if (Serial.available() > 0) {
    char ch = Serial.read();
    
    // 개행 문자를 만나면 명령 처리
    if (ch == '\n' || ch == '\r') {
      if (cmdIndex > 0) {  // 버퍼에 데이터가 있을 때만 처리
        cmdBuffer[cmdIndex] = '\0';
        processCommand();
        cmdIndex = 0;  // 버퍼 초기화
      }
      return;
    }
    
    // 버퍼에 문자 추가 (공백 무시)
    if (ch == ' ') {
      return;
    }
    
    if (cmdIndex < 19) {
      cmdBuffer[cmdIndex] = ch;
      cmdIndex++;
    } else {
      // 버퍼 오버플로우 방지
      cmdIndex = 0;
      Serial.println("[오류] 명령이 너무 깁니다");
      while (Serial.available() > 0) {
        Serial.read();
      }
    }
  }
}

/**
 * 명령 처리
 */
void processCommand() {
  Serial.print("[수신] ");
  Serial.println(cmdBuffer);
  
  // 최소 길이 확인 (arm0_0 = 6자)
  int len = 0;
  while (cmdBuffer[len] != '\0') len++;
  
  if (len < 6) {
    Serial.println("[오류] 명령이 너무 짧습니다 (예: arm0_90)");
    return;
  }
  
  // "arm" 명령인지 확인
  if (cmdBuffer[0] != 'a' || cmdBuffer[1] != 'r' || cmdBuffer[2] != 'm') {
    Serial.println("[오류] arm으로 시작해야 합니다");
    return;
  }
  
  // 서보 번호 추출 (arm0, arm1, arm2, arm3)
  int servoNum = cmdBuffer[3] - '0';
  if (servoNum < 0 || servoNum > 3) {
    Serial.println("[오류] 서보 번호는 0~3입니다");
    return;
  }
  
  // '_' 구분자 확인
  if (cmdBuffer[4] != '_') {
    Serial.println("[오류] arm0_90 형식으로 입력하세요");
    return;
  }
  
  // 각도 추출 (arm0_90 형식)
  int angle = 0;
  bool hasDigit = false;
  
  for (int i = 5; cmdBuffer[i] != '\0'; i++) {
    if (cmdBuffer[i] >= '0' && cmdBuffer[i] <= '9') {
      angle = angle * 10 + (cmdBuffer[i] - '0');
      hasDigit = true;
    } else if (cmdBuffer[i] == '_') {
      break;  // 마지막 '_'는 무시
    }
  }
  
  if (!hasDigit) {
    Serial.println("[오류] 각도 값이 없습니다");
    return;
  }
  
  // 각도 범위 확인
  if (angle < minAngles[servoNum] || angle > maxAngles[servoNum]) {
    Serial.print("[오류] 각도 범위 초과 (");
    Serial.print(minAngles[servoNum]);
    Serial.print(" ~ ");
    Serial.print(maxAngles[servoNum]);
    Serial.println(")");
    return;
  }
  
  // 서보 이동
  const char* names[] = {"베이스", "팔꿈치", "손목", "그립"};
  Serial.print("[실행] ");
  Serial.print(names[servoNum]);
  Serial.print(": ");
  Serial.print(angles[servoNum]);
  Serial.print("° → ");
  Serial.print(angle);
  Serial.println("°");
  
  // 부드럽게 이동
  int start = angles[servoNum];
  int end = angle;
  
  if (start < end) {
    for (int a = start; a <= end; a++) {
      servo[servoNum].write(a);
      delay(10);
    }
  } else {
    for (int a = start; a >= end; a--) {
      servo[servoNum].write(a);
      delay(10);
    }
  }
  
  angles[servoNum] = angle;
  
  Serial.print("[완료] ");
  Serial.print(names[servoNum]);
  Serial.print(" = ");
  Serial.print(angle);
  Serial.println("°\n");
}

