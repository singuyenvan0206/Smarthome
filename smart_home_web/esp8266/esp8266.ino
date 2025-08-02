#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <DHT.h>

#define WIFI_SSID "Tang 1"
#define WIFI_PASSWORD "12345689"
#define FIREBASE_HOST "smarthome-3b496-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "CNTAqHKXPxWnZg9rztuTHLbb0fVbzDyE6nQToFXE"

#define DHTPIN D1
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Firebase objects
FirebaseData fbdo;
FirebaseData streamLed; // stream for led
FirebaseConfig config;
FirebaseAuth auth;

SoftwareSerial unoSerial(D7, D8); // RX, TX (ESP RX=D7, TX=D8)

float lastThresholdTemp = -1000;  // giá trị mặc định khác biệt
float lastTemp = NAN;
String lastLEDCommand = "", lastServo1Cmd = "", lastServo2Cmd = "";
bool autoControlServo2 = false;

void setup() {
  Serial.begin(9600);
  unoSerial.begin(9600);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Start streaming LED control so updates are immediate
  if (!Firebase.beginStream(streamLed, "/control/led")) {
    Serial.print("Stream LED start failed: ");
    Serial.println(streamLed.errorReason());
  } else {
    Serial.println("Streaming /control/led");
  }

  delay(500);
  fetchPasswordFromFirebase(); // initial sync password
}

void loop() {
  // 1. Cảm biến
  static float lastThreshold = -1;  // lưu threshold cũ

float thresholdTemp = 27.0;
if (Firebase.getFloat(fbdo, "/config/servo2_threshold")) {
  thresholdTemp = fbdo.floatData();
}

// Nếu ngưỡng thay đổi và đang bật auto thì so sánh lại ngay
if (autoControlServo2 && thresholdTemp != lastThreshold) {
  if (t > thresholdTemp) {
    unoSerial.println("SERVO2_OPEN");
    lastServo2Cmd = "AUTO_OPEN";
  } else {
    unoSerial.println("SERVO2_CLOSE");
    lastServo2Cmd = "AUTO_CLOSE";
  }
}
lastThreshold = thresholdTemp;



  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (Firebase.ready()) {
    if (!isnan(t) && !isnan(h)) {
      Firebase.setFloat(fbdo, "/sensor/temperature", t);
      Firebase.setFloat(fbdo, "/sensor/humidity", h);
    }
  }

  // 2. Xử lý stream LED từ Firebase (ngay lập tức)
  if (Firebase.readStream(streamLed)) {
    if (streamLed.streamPath() == "/control/led" && streamLed.dataType() == "string") {
      String cmd = streamLed.stringData();
      cmd.toUpperCase();
      if (cmd != lastLEDCommand) {
        unoSerial.println(cmd == "ON" ? "LED_ON" : "LED_OFF");
        lastLEDCommand = cmd;
        Serial.println("🔥 Firebase stream cập nhật LED: " + cmd);
      }
    }
  } else {
    // nếu stream gặp lỗi, có thể thử khởi động lại
    if (streamLed.httpCode() != 0) {
      // optional: reconnect stream on failure
      // Firebase.endStream(streamLed);
      // Firebase.beginStream(streamLed, "/control/led");
    }
  }

  // 3. Nhận dữ liệu từ UNO
  if (unoSerial.available()) {
    String line = unoSerial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) ; // skip empty
    Serial.println("[UNO] " + line);
  if (line == "SERVO1_OPEN" && lastServo1Cmd != "OPEN") {
    Firebase.setString(fbdo, "/control/servo1", "OPEN");
    lastServo1Cmd = "OPEN";
  } else if (line == "SERVO1_CLOSE" && lastServo1Cmd != "CLOSE") {
    Firebase.setString(fbdo, "/control/servo1", "CLOSE");
    lastServo1Cmd = "CLOSE";
  }

  if (line == "SERVO2_OPEN" && lastServo2Cmd != "OPEN") {
    Firebase.setString(fbdo, "/control/servo2", "OPEN");
    lastServo2Cmd = "OPEN";
  } else if (line == "SERVO2_CLOSE" && lastServo2Cmd != "CLOSE") {
    Firebase.setString(fbdo, "/control/servo2", "CLOSE");
    lastServo2Cmd = "CLOSE";
  }

    if (line.startsWith("LOG_SERVO1:")) {
      String timeStr = line.substring(String("LOG_SERVO1:").length());
      unsigned long logKey = millis();
      String path = "/log/servo1/" + String(logKey);
      Firebase.setString(fbdo, path, timeStr);
    }

    if (line.startsWith("NEW_PASS:")) {
      String newPass = line.substring(9);
      Firebase.setString(fbdo, "/config/password", newPass);
    }

    if (line == "LED_ON" && lastLEDCommand != "ON") {
      Firebase.setString(fbdo, "/control/led", "ON");
      lastLEDCommand = "ON";
      Serial.println("Firebase cập nhật từ UNO: ON");
    } else if (line == "LED_OFF" && lastLEDCommand != "OFF") {
      Firebase.setString(fbdo, "/control/led", "OFF");
      lastLEDCommand = "OFF";
      Serial.println("Firebase cập nhật từ UNO: OFF");
    }
  }

  // 4. Điều khiển Servo2 tự động theo nhiệt độ
  if (Firebase.getBool(fbdo, "/control/auto_servo2")) {
    autoControlServo2 = fbdo.boolData();
  }

  if (autoControlServo2) {
    if (t > thresholdTemp && lastServo2Cmd != "AUTO_OPEN") {
      unoSerial.println("SERVO2_OPEN");
      lastServo2Cmd = "AUTO_OPEN";
    } else if (t <= thresholdTemp && lastServo2Cmd != "AUTO_CLOSE") {
      unoSerial.println("SERVO2_CLOSE");
      lastServo2Cmd = "AUTO_CLOSE";
    }
  } else {
    if (Firebase.getString(fbdo, "/control/servo2")) {
      String cmd = fbdo.stringData();
      if (cmd != lastServo2Cmd) {
        unoSerial.println(cmd == "OPEN" ? "SERVO2_OPEN" : "SERVO2_CLOSE");
        lastServo2Cmd = cmd;
      }
    }
  }

  // 5. Đồng bộ servo1 từ Firebase
  if (Firebase.getString(fbdo, "/control/servo1")) {
    String cmd = fbdo.stringData();
    if (cmd != lastServo1Cmd) {
      unoSerial.println(cmd == "OPEN" ? "SERVO1_OPEN" : "SERVO1_CLOSE");
      lastServo1Cmd = cmd;
    }
  }

  // 6. Gửi mật khẩu xuống UNO (luôn lấy mới)
  if (Firebase.getString(fbdo, "/config/password")) {
    String password = fbdo.stringData();
    unoSerial.println("#PWD:" + password);
  }
  delay(100);
}

void fetchPasswordFromFirebase() {
  if (Firebase.getString(fbdo, "/config/password")) {
    String pass = fbdo.stringData();
    unoSerial.println("PASS:" + pass);
  }
  if (Firebase.getString(fbdo, "/config/admin")) {
    String admin = fbdo.stringData();
    unoSerial.println("ADMIN:" + admin);
  }
}
