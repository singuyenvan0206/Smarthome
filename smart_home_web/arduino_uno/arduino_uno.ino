#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <RTClib.h>
#include <IRremote.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

#define IR_RECEIVE_PIN 11

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9,8,7,6};
byte colPins[COLS] = {5,4,3,2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo doorServo, doorServo2;

#define LED_PIN1 A3   
#define SERVO_PIN 10
#define SERVO2_PIN A1
#define BUZZER_PIN 12
#define LED_PIN2 A2
#define BUTTON_PIN A0

bool ledState = false;
bool servo2State = false;
bool servoState = false;
unsigned long lastButtonPress = 0;

String correctCode = "1234", inputCode = "";
String adminCode = "0000";

bool changingPassword = false;
String step = "";
String adminInput = "", oldInput = "", newInput = "", confirmInput = "";

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  rtc.begin();
  rtc.adjust(DateTime(2025, 8, 2, 2,37, 0));

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);

  doorServo.attach(SERVO_PIN);
  doorServo2.attach(SERVO2_PIN);
  doorServo.write(0);
  doorServo2.write(0);

  lcd.setCursor(0, 0);
  lcd.print("Nhap mat khau:");
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver ready (UNO)");
}

void loop() {
  // IR Remote
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    if (cmd != 0) {  // tránh in ra 0x0
    Serial.print("IR Code: 0x");
    Serial.println(cmd, HEX);
    }
    if (cmd == 0x45) { 
      ledState = !ledState;
      digitalWrite(LED_PIN1, ledState ? HIGH : LOW);
      Serial.println(ledState ? "LED_ON" : "LED_OFF");
    }
    if (cmd == 0x47) { 
    servo2State = !servo2State;
    doorServo2.write(servo2State ? 180 : 0);
    Serial.println(servo2State ? "SERVO2_OPEN" : "SERVO2_CLOSE");
  }
    if (cmd == 0x40) { 
      servoState = !servoState;
      doorServo.write(servoState ? 180 : 0);
      Serial.println(servoState ? "SERVO1_OPEN" : "SERVO1_CLOSE");
      if (servoState) {
        DateTime now = rtc.now();
        String timeLog = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + " - " +
                        String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
        Serial.println("LOG_SERVO1:" + timeLog);
  }
    }
    IrReceiver.resume();
  }

  // Nhận từ ESP
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("PASS:")) {
      correctCode = cmd.substring(5);
      lcd.clear(); lcd.print("Cap nhat PIN");
      delay(1000);
      lcd.clear(); lcd.print("Nhap mat khau:");
    } else if (cmd.startsWith("ADMIN:")) {
      adminCode = cmd.substring(6);
    } else if (cmd == "LED_ON") {
      ledState = true;
      digitalWrite(LED_PIN1, HIGH);
      Serial.println("LED_ON"); // phản hồi để ESP biết
    } else if (cmd == "LED_OFF") {
      ledState = false;
      digitalWrite(LED_PIN1, LOW);
      Serial.println("LED_OFF");
    } else if (cmd == "SERVO1_OPEN") {
      doorServo.write(90);
    } else if (cmd == "SERVO1_CLOSE") {
      doorServo.write(0);
    } else if (cmd == "SERVO2_OPEN") {
      doorServo2.write(90);
    } else if (cmd == "SERVO2_CLOSE") {
      doorServo2.write(0);
    }
  }

  // Nút vật lý bật/tắt LED
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > 300) {
    ledState = !ledState;
    digitalWrite(LED_PIN1, ledState ? HIGH : LOW);
    Serial.println(ledState ? "LED_ON" : "LED_OFF"); // gửi ESP
    lastButtonPress = millis();
  }

  // Bàn phím nhập PIN / đổi mật khẩu
  char key = keypad.getKey();
  if (key) {
    if (key == 'A' && !changingPassword) {
      changingPassword = true;
      step = "admin";
      lcd.clear();
      lcd.print("Admin code:");
      return;
    }

    if (changingPassword) {
      if (key == '#') {
        if (step == "admin") {
          if (adminInput == adminCode) {
            step = "old";
            lcd.clear(); lcd.print("Old PIN:");
          } else {
            lcd.clear(); lcd.print("Sai admin!");
            delay(2000);
            resetChangePass();
          }
        } else if (step == "old") {
          if (oldInput == correctCode) {
            step = "new";
            lcd.clear(); lcd.print("New PIN:");
          } else {
            lcd.clear(); lcd.print("Sai PIN cu!");
            delay(2000);
            resetChangePass();
          }
        } else if (step == "new") {
          step = "confirm";
          lcd.clear(); lcd.print("Xac nhan:");
        } else if (step == "confirm") {
          if (confirmInput == newInput) {
            correctCode = newInput;
            lcd.clear(); lcd.print("Doi thanh cong!");
            Serial.println("NEW_PASS:" + newInput);
          } else {
            lcd.clear(); lcd.print("Khong khop!");
          }
          delay(2000);
          resetChangePass();
        }
      } else if (key == '*') {
        if (step == "admin" && adminInput.length() > 0) adminInput.remove(adminInput.length() - 1);
        else if (step == "old" && oldInput.length() > 0) oldInput.remove(oldInput.length() - 1);
        else if (step == "new" && newInput.length() > 0) newInput.remove(newInput.length() - 1);
        else if (step == "confirm" && confirmInput.length() > 0) confirmInput.remove(confirmInput.length() - 1);
      } else {
        if (step == "admin") adminInput += key;
        else if (step == "old") oldInput += key;
        else if (step == "new") newInput += key;
        else if (step == "confirm") confirmInput += key;
      }
      return;
    }

    if (key == '#') {
      if (inputCode == correctCode) unlockDoor();
      else wrongAccess();
      inputCode = "";
      lcd.clear(); lcd.print("Nhap mat khau:");
    } else if (key == '*') {
      if (inputCode.length() > 0) inputCode.remove(inputCode.length() - 1);
    } else {
      inputCode += key;
      lcd.setCursor(inputCode.length() - 1, 1);
      lcd.print("*");
    }
  }
}

void unlockDoor() {
  lcd.clear();
  lcd.print("Access Granted");
  digitalWrite(LED_PIN2, HIGH);
  tone(BUZZER_PIN, 1000, 200);
  doorServo.write(90);
  delay(2000);
  doorServo.write(0);
  digitalWrite(LED_PIN2, LOW);
  DateTime now = rtc.now();
  String timeLog = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + " - " +
                   String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  Serial.println("LOG_SERVO1:" + timeLog);
}

void wrongAccess() {
  lcd.clear();
  lcd.print("Access Denied");
  tone(BUZZER_PIN, 500, 1000);
}

void resetChangePass() {
  changingPassword = false;
  adminInput = oldInput = newInput = confirmInput = "";
  lcd.clear();
  lcd.print("Nhap mat khau:");
}
