#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// Define pins
const int irSensorPin = 8;
const int buzzerPin = 9;
const int servoPin = 10;
const int espRxPin = 12; // Arduino RX (ESP32 TX 17)
const int espTxPin = 11; // Arduino TX (ESP32 RX 16)

SoftwareSerial espSerial(espRxPin, espTxPin);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {0, 2, 3, 4};
byte colPins[COLS] = {5, 6, 7};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

struct Parcel {
  String id = "0";
  String otp = "0";
} parcel;

String userInput = "";
bool boxOpen = false;
const int LOCK_POSITION = 5;
const int OPEN_POSITION = 90;

// Non-blocking timer variables
unsigned long lastInputTime = 0;
const unsigned long inputTimeout = 10000; // 10 seconds timeout for OTP input

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  lcd.begin(16, 2);
  lcd.backlight();
  myServo.attach(servoPin);
  myServo.write(LOCK_POSITION);
  pinMode(irSensorPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  displayParcelVault();
}

void loop() {
  if (espSerial.available()) {
    String data = espSerial.readStringUntil('\n');
    Serial.println("Received from ESP32: " + data);
    parseParcelData(data);
  }

  char key = keypad.getKey();
  if (key) {
    handleKeypadInput(key);
    lastInputTime = millis(); // Reset the input timeout timer
  }

  // Check for input timeout
  if (parcel.id != "0" && parcel.otp != "0" && userInput.length() > 0 &&
      millis() - lastInputTime > inputTimeout) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OTP Timeout!");
    delay(2000);
    resetParcel();
    displayParcelVault();
  }

  if (boxOpen && digitalRead(irSensorPin) == LOW) {
    lockBox();
    sendDeliveryStatus();
    resetParcel();
    displayParcelVault();
  }
}

void parseParcelData(String data) {
  int commaIndex = data.indexOf(',');
  if (commaIndex != -1) {
    parcel.id = data.substring(0, commaIndex);
    parcel.otp = data.substring(commaIndex + 1);
    Serial.println("Parsed id: " + parcel.id);
    Serial.println("Parsed otp: " + parcel.otp);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter OTP:");
    userInput = ""; // Reset user input
    lastInputTime = millis(); // Start the input timeout timer
  } else {
    Serial.println("Invalid data: " + data);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Data Error");
    delay(2000);
    displayParcelVault();
  }
}

void handleKeypadInput(char key) {
  if (parcel.id == "0") return; // Ignore input if no parcel is active

  if (key == '*') {
    if (userInput.length() > 0) {
      userInput.remove(userInput.length() - 1);
    }
  } else if (isdigit(key)) {
    if (userInput.length() < 4) {
      userInput += key;
    }
  } else if (key == '#') {
    if (userInput.length() == 4) {
      if (userInput == parcel.otp) {
        playSuccessSound();
        openBox();
        userInput = "";
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Drop Parcel &");
        lcd.setCursor(0, 1);
        lcd.print("Close Lid...");
      } else {
        playFailureSound();
        userInput = "";
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wrong OTP!");
        delay(1000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter OTP:");
      }
    }
  }

  // Update LCD with current input
  lcd.setCursor(0, 1);
  lcd.print("    "); // Clear previous input
  lcd.setCursor(0, 1);
  lcd.print(userInput);
}

void openBox() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Opening Box...");
  playOpeningMelody();
  myServo.write(OPEN_POSITION);
  delay(1000);
  noTone(buzzerPin);
  boxOpen = true;
}

void lockBox() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Locking Box...");
  playClosingMelody();
  myServo.write(LOCK_POSITION);
  delay(1000);
  noTone(buzzerPin);
  boxOpen = false;
}

void sendDeliveryStatus() {
  String json = "{\"id\":" + parcel.id + ",\"status\":\"delivered\"}";
  espSerial.println(json);
  Serial.println("Sent to ESP32: " + json);
}

void resetParcel() {
  parcel.id = "0";
  parcel.otp = "0";
  userInput = "";
  boxOpen = false;
}

void displayParcelVault() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ParcelVault");
}

void playSuccessSound() {
  tone(buzzerPin, 1000, 200);
  delay(200);
  noTone(buzzerPin);
}

void playFailureSound() {
  tone(buzzerPin, 500, 200);
  delay(200);
  noTone(buzzerPin);
}

void playOpeningMelody() {
  int melody[] = {523, 659, 784};
  int durations[] = {200, 200, 200};
  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, melody[i], durations[i]);
    delay(durations[i]);
    noTone(buzzerPin);
    delay(50);
  }
}

void playClosingMelody() {
  int melody[] = {784, 659, 523};
  int durations[] = {200, 200, 200};
  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, melody[i], durations[i]);
    delay(durations[i]);
    noTone(buzzerPin);
    delay(50);
  }
}