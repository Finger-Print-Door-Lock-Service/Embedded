#include <WiFi.h>
#include <WebServer.h>

// SoftAP 설정
const char* ssid = "ESP32-Config";
const char* password = "12345678";

WebServer server(80);
bool softAPActive = false; // SoftAP 상태 추적

// HTML 페이지
const char* htmlPage = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>사용자 정보 입력</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 400px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h2 {
            text-align: center;
            color: #333;
            margin-bottom: 30px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #555;
            font-weight: bold;
        }
        input[type="text"], input[type="email"], input[type="password"] {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 16px;
            box-sizing: border-box;
        }
        input[type="submit"] {
            width: 100%;
            background-color: #4CAF50;
            color: white;
            padding: 12px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
            margin-top: 10px;
        }
        input[type="submit"]:hover {
            background-color: #45a049;
        }
        .status {
            margin-top: 20px;
            padding: 10px;
            text-align: center;
            border-radius: 4px;
        }
        .success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>사용자 정보 입력</h2>
        <form action="/submit" method="POST">
            <div class="form-group">
                <label for="name">이름:</label>
                <input type="text" id="name" name="name" required>
            </div>
            
            <div class="form-group">
                <label for="email">이메일:</label>
                <input type="email" id="email" name="email" required>
            </div>
            
            <div class="form-group">
                <label for="password">비밀번호:</label>
                <input type="password" id="password" name="password" required>
            </div>
            
            <input type="submit" value="전송">
        </form>
        
        <div id="status"></div>
    </div>
</body>
</html>
)";

// 성공 페이지
const char* successPage = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>전송 완료</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 400px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            text-align: center;
        }
        .success {
            color: #155724;
            margin-bottom: 20px;
        }
        .back-btn {
            background-color: #007bff;
            color: white;
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            text-decoration: none;
            display: inline-block;
        }
        .back-btn:hover {
            background-color: #0056b3;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2 class="success">✓ 정보가 성공적으로 전송되었습니다!</h2>
        <a href="/" class="back-btn">다시 입력하기</a>
    </div>
</body>
</html>
)";

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600, SERIAL_8N1, 16, 17); // espMain과 동일하게 16, 17로 변경
    
    Serial.println("ESP32 WiFi - 대기 모드");
    Serial.println("espMain으로부터 SoftAP 시작 명령 대기 중...");
}

void loop() {
    // espMain으로부터 명령 수신
    if (Serial1.available()) {
        String command = Serial1.readStringUntil('\n');
        command.trim();
        
        Serial.println("espMain으로부터 수신: '" + command + "'"); // 디버그 추가
        
        if (command == "START_SOFTAP") {
            Serial.println("SoftAP 시작 명령 수신");
            startSoftAP();
        } else {
            Serial.println("알 수 없는 명령: " + command); // 디버그 추가
        }
    }
    
    // SoftAP가 활성화된 경우에만 웹서버 처리
    if (softAPActive) {
        server.handleClient();
    }
}

// 메인 페이지 핸들러
void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

// 폼 제출 핸들러
void handleSubmit() {
    if (server.hasArg("name") && server.hasArg("email") && server.hasArg("password")) {
        String name = server.arg("name");
        String email = server.arg("email");
        String password = server.arg("password");
        
        // 입력 데이터 검증
        if (name.length() > 0 && email.length() > 0 && password.length() > 0) {
            // UART로 데이터 전송 (JSON 형태)
            String jsonData = "{\"name\":\"" + name + "\",\"email\":\"" + email
             + "\",\"password\":\"" + password + "\"}";
            Serial1.println(jsonData);
            
            // 시리얼 모니터에도 출력
            Serial.println("Sent data: " + jsonData);
            
            // 성공 페이지 응답
            server.send(200, "text/html", successPage);
        } else {
            // 빈 필드가 있는 경우
            server.send(400, "text/plain", "모든 필드를 입력해주세요.");
        }
    } else {
        // 필수 파라미터가 없는 경우
        server.send(400, "text/plain", "필수 정보가 누락되었습니다.");
    }
}

// SoftAP 시작 함수
void startSoftAP() {
    if (!softAPActive) {
        // SoftAP 시작
        WiFi.softAP(ssid, password);
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(IP);
        
        // 웹서버 라우트 설정
        server.on("/", HTTP_GET, handleRoot);
        server.on("/submit", HTTP_POST, handleSubmit);
        
        // 서버 시작
        server.begin();
        Serial.println("HTTP server started");
        
        softAPActive = true;
        Serial.println("SoftAP 활성화 완료");
    } else {
        Serial.println("SoftAP 이미 활성화됨");
    }
}