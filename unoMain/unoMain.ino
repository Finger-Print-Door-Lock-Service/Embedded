#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <Servo.h>

// 지문센서용 SoftwareSerial (D2, D3) - 이것만 사용
SoftwareSerial mySerial(2, 3);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// 서보모터 설정
Servo doorServo;
const int servoPin = 7;

// ESP32는 하드웨어 시리얼(Serial) 사용

uint8_t id;
bool enrollmentMode = false;
bool authenticationMode = true;

void setup() {
  Serial.begin(9600);     // ESP32와 통신
  mySerial.begin(57600);  // 지문센서와 통신
  
  // 서보모터 초기화
  doorServo.attach(servoPin);
  doorServo.write(0); // 초기 위치 (0도)
  
  delay(500);
  
  // 지문센서 초기화
  finger.begin(57600);
  delay(100);

  if (finger.verifyPassword()) {
    finger.getParameters();
    Serial.println("READY"); // 간단한 준비 신호만
  }
  
  delay(100);
}

void loop() {
  // START_ENROLLMENT 및 OPEN_DOOR_KEEP 수신
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    // 디버깅: 받은 명령 확인
    if (command.length() > 0) {
      Serial.print("CMD:");
      Serial.println(command);
      
      if (command == "START_ENROLLMENT") {
        Serial.println("STARTING");
        authenticationMode = false;
        enrollmentMode = true;
        startEnrollment();
      } else if (command == "OPEN_DOOR_KEEP") {
        Serial.println("DOOR_OPEN_KEEP");
        keepDoorOpen();
      }
    }
  }
  
  // 지문 인증만 (등록 중이 아닐 때)
  if (authenticationMode && !enrollmentMode) {
    checkFingerprint();
  }
}

void startEnrollment() {
  // 자동으로 빈 ID 찾기
  id = findEmptyID();
  if (id == 0) {
    Serial.println("ERROR:NO_EMPTY_SLOTS");
    enrollmentMode = false;
    authenticationMode = true;
    return;
  }
  // 지문 등록 프로세스 시작
  if (getFingerprintEnroll()) {
    Serial.print("ENROLLMENT_SUCCESS:");
    Serial.println(id);
    
    // 등록 성공 후 손가락 제거 안내
    Serial.println("REMOVE_FINGER");
    delay(3000); // 3초 대기
    
    // 손가락이 완전히 제거될 때까지 대기
    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      delay(100);
    }
    Serial.println("READY_FOR_AUTH"); // 인증 준비 완료
  } else {
    Serial.println("ENROLLMENT_FAILED");
  }
  enrollmentMode = false;
  authenticationMode = true;
}

uint8_t findEmptyID() {
  for (uint8_t i = 1; i <= finger.capacity; i++) {
    if (finger.loadModel(i) != FINGERPRINT_OK) {
      return i;
    }
  }
  return 0;
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  
  // 첫 번째 지문 스캔 - 성공할 때까지 무한 반복
  while (true) {
    Serial.println("PLACE_FINGER_1");
    
    unsigned long startTime = millis();
    p = -1;
    
    while (p != FINGERPRINT_OK) {
      p = finger.getImage();
      if (p == FINGERPRINT_NOFINGER) {
        delay(100);
        // 30초 타임아웃 시 다시 PLACE_FINGER_1부터
        if (millis() - startTime > 30000) {
          Serial.println("TIMEOUT_TRY_AGAIN");
          break; // while 루프 탈출하고 다시 PLACE_FINGER_1
        }
      } else if (p != FINGERPRINT_OK) {
        Serial.println("TRY_AGAIN");
        delay(1000);
        break; // while 루프 탈출하고 다시 PLACE_FINGER_1
      }
    }
    
    // 성공한 경우만 다음 단계로
    if (p == FINGERPRINT_OK) {
      Serial.println("GOT_IMAGE_1");
      delay(3000);
      
      if (finger.image2Tz(1) == FINGERPRINT_OK) {
        break; // 첫 번째 스캔 완료
      } else {
        Serial.println("TRY_AGAIN");
        delay(1000);
      }
    }
  }

  Serial.println("REMOVE_FINGER");
  delay(3000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    delay(100);
  }
  
  // 두 번째 지문 스캔 - 성공할 때까지 무한 반복
  while (true) {
    Serial.println("PLACE_FINGER_2");
    
    unsigned long startTime = millis();
    p = -1;
    
    while (p != FINGERPRINT_OK) {
      p = finger.getImage();
      if (p == FINGERPRINT_NOFINGER) {
        delay(100);
        // 30초 타임아웃 시 다시 PLACE_FINGER_2부터
        if (millis() - startTime > 30000) {
          Serial.println("TIMEOUT_TRY_AGAIN");
          break; // while 루프 탈출하고 다시 PLACE_FINGER_2
        }
      } else if (p != FINGERPRINT_OK) {
        Serial.println("TRY_AGAIN");
        delay(1000);
        break; // while 루프 탈출하고 다시 PLACE_FINGER_2
      }
    }
    
    // 성공한 경우만 다음 단계로
    if (p == FINGERPRINT_OK) {
      Serial.println("GOT_IMAGE_2");
      delay(3000);
      
      if (finger.image2Tz(2) == FINGERPRINT_OK) {
        break; // 두 번째 스캔 완료
      } else {
        Serial.println("TRY_AGAIN");
        delay(1000);
      }
    }
  }

  Serial.println("CREATING_MODEL");
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("MODEL_ERROR_RESTART");
    // 모델 생성 실패 시 처음부터 다시
    return getFingerprintEnroll();
  }

  Serial.println("STORING_MODEL");
  if (finger.storeModel(id) == FINGERPRINT_OK) {
    Serial.println("STORED_SUCCESS");
    return true;
  } else {
    Serial.println("STORE_ERROR_RESTART");
    // 저장 실패 시 처음부터 다시
    return getFingerprintEnroll();
  }
}

void checkFingerprint() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    return;
  }
  // 지문을 더 오래 누르고 있어도 되도록 2초 대기
  delay(2000);
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.println("AUTH_FAILED");
    return;
  }
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("AUTH_SUCCESS:");
    Serial.println(finger.fingerID);
    // 지문 인증 성공 시 서보모터 제어
    openDoor();
    delay(2000); // 인증 후 2초 대기 (연속 인증 방지)
  } else {
    Serial.println("AUTH_FAILED");
    delay(1000);
  }
}

void openDoor() {
  Serial.println("DOOR_OPENING");
  
  // 서보모터 90도 회전 (문 열기)
  doorServo.write(90);
  delay(500); // 서보모터 움직임 완료 대기
  
  Serial.println("DOOR_OPENED");
  
  // 10초 대기
  delay(10000);
  
  Serial.println("DOOR_CLOSING");
  
  // 서보모터 0도로 복귀 (문 닫기)
  doorServo.write(0);
  delay(500); // 서보모터 움직임 완료 대기
  
  Serial.println("DOOR_CLOSED");
}
void keepDoorOpen() {
  Serial.println("DOOR_OPENING_PERMANENT");
  
  // 서보모터 90도 회전 (문 열기)
  doorServo.write(90);
  delay(500); // 서보모터 움직임 완료 대기
  
  Serial.println("DOOR_OPENED_PERMANENT");
  // 닫지 않고 계속 열린 상태 유지
}