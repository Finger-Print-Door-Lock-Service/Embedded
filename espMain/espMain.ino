#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <time.h>

// WiFi 설정
const char* ssid = "DaebukHwakSeongi";
const char* password = "skfktkfkd1;";

// 서버 설정
const char* server_url = "http://192.168.0.9:8080/devices/device";
const char* fingerprint_enrollment_url = "http://192.168.0.9:8080/members/complete";
const char* fingerprint_auth_url = "http://192.168.0.9:8080/logs";

// MQTT 설정
const char* mqtt_server = "192.168.0.9";
const int mqtt_port = 1883;
const char* mqtt_topic_request = "fingerprint/0";

// NTP 설정
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;  // 한국 시간 (UTC+9)
const int daylightOffset_sec = 0;

WiFiClient espClient;
PubSubClient client(espClient);

// Arduino와 통신용 시리얼
HardwareSerial arduinoSerial(2);

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600, SERIAL_8N1, 16, 17); // espWifi 통신
    arduinoSerial.begin(9600, SERIAL_8N1, 25, 26); // Arduino 통신
    
    setupWiFi();
    
    // NTP 시간 동기화
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("NTP 시간 동기화 중...");
    delay(2000);
    
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    
    Serial.println("시스템 준비 완료");
}

void loop() {
    // MQTT 연결 유지
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();
    
    // espWifi 데이터 수신
    if (Serial1.available()) {
        String receivedData = Serial1.readStringUntil('\n');
        receivedData.trim();
        
        if (receivedData.length() > 0) {
            parseUserData(receivedData);
        }
    }
    
    // Arduino 응답 수신
    if (arduinoSerial.available()) {
        String response = arduinoSerial.readStringUntil('\n');
        response.trim();
        
        if (response.length() > 0) {
            handleFingerprintResponse(response);
        }
    }
}

void parseUserData(String jsonString) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonString);
    
    if (error) {
        return;
    }
    
    String name = doc["name"];
    String email = doc["email"];
    String password = doc["password"];
    
    if (validateUserData(name, email, password)) {
        sendDeviceRegisterToServer(name, email, password);
    }
}

bool validateUserData(String name, String email, String password) {
    return (name.length() >= 2 && 
            email.indexOf("@") != -1 && 
            email.indexOf(".") != -1 && 
            password.length() >= 4);
}

void startFingerprintEnrollment() {
    arduinoSerial.println("START_ENROLLMENT");
    arduinoSerial.flush();
}

void handleFingerprintResponse(String response) {
    if (response.startsWith("ENROLLMENT_SUCCESS:")) {
        String idStr = response.substring(19);
        int fingerprintID = idStr.toInt();
        sendFingerprintEnrollmentToServer(fingerprintID);
        
    } else if (response.startsWith("AUTH_SUCCESS:")) {
        String idStr = response.substring(13);
        int fingerprintID = idStr.toInt();
        sendFingerprintAuthToServer(fingerprintID, true);
        
    } else if (response == "AUTH_FAILED") {
        sendFingerprintAuthToServer(-1, false);
    }
}

void sendDeviceRegisterToServer(String name, String email, String password) {
    HTTPClient http;
    http.begin(server_url);
    http.addHeader("Content-Type", "application/json");
    
    String macAddress = WiFi.macAddress();
    
    DynamicJsonDocument doc(512);
    doc["macAddress"] = macAddress;
    doc["name"] = name;
    doc["email"] = email;
    doc["password"] = password;
    doc["deviceIdForMqtt"] = 0;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    http.POST(jsonString);
    http.end();
}

void sendFingerprintEnrollmentToServer(int fingerprintID) {
    HTTPClient http;
    http.begin(fingerprint_enrollment_url);
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument doc(256);
    doc["deviceIdForMqtt"] = 0;  // 디바이스 ID 
    doc["id"] = fingerprintID;   // 지문 ID (올바른 키명)
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    http.POST(jsonString);
    http.end();
}

void sendFingerprintAuthToServer(int fingerprintID, bool success) {
    HTTPClient http;
    http.begin(fingerprint_auth_url);
    http.addHeader("Content-Type", "application/json");
    
    // 현재 시간을 NTP로 가져오기
    String timestamp = getCurrentTimestamp();
    
    DynamicJsonDocument doc(512);
    doc["deviceIdForMqtt"] = 0;
    doc["memberIdOnDevice"] = fingerprintID;
    doc["result"] = success;
    doc["timestamp"] = timestamp;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    http.POST(jsonString);
    http.end();
}

void setupWiFi() {
    Serial.print("WiFi 연결 중...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi 연결 완료!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("MQTT 연결 시도...");
        if (client.connect("ESP32_Main")) {
            Serial.println("성공!");
            client.subscribe(mqtt_topic_request);
            Serial.println("MQTT 준비 완료");
        } else {
            Serial.println("실패");
            delay(5000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.print("MQTT 수신: ");
    Serial.println(message);
    
    int command = message.toInt();
    
    if (command == 1000) {
        Serial.println("→ 기기 등록 요청");
        handleDeviceRegisterRequest();
    } else if (command == 1001) {
        Serial.println("→ 지문 등록 요청");
        handleFingerprintRegisterRequest();
    } else if (command == 1002) {
        Serial.println("→ 문 열기 요청");
        handleDoorOpenRequest();
    }
}

void handleDeviceRegisterRequest() {
    Serial.println("espWifi에 SoftAP 시작 명령 전송");
    Serial1.println("START_SOFTAP");
}

void handleFingerprintRegisterRequest() {
    Serial.println("Arduino에 지문 등록 명령 전송");
    startFingerprintEnrollment();
}

void handleDoorOpenRequest() {
    Serial.println("Arduino에 문 열기 명령 전송");
    arduinoSerial.println("OPEN_DOOR_KEEP");
    arduinoSerial.flush();
}

String getCurrentTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("시간 가져오기 실패");
        // 실패 시 임시 시간 반환
        return "2024-12-17T12:00:00.000";
    }
    
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000", &timeinfo);
    return String(timestamp);
}