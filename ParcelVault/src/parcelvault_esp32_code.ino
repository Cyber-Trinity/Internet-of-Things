#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// WiFi credentials
const char* ssid = "ParcelVault";
const char* password = "Parcelvault123";

// Supabase API details
const char* supabaseUrl = "https://ylmulnevakkhmoocxnhk.supabase.co/rest/v1/deliveries?select=*&delivery_status=eq.pending&limit=1";
const char* apiKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsbXVsbmV2YWtraG1vb2N4bmhrIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDEwNjA1MDEsImV4cCI6MjA1NjYzNjUwMX0.wzxz2XpKD3cUJbYcWDLiUg6mpBjPOb9kTXocnItPh3A";

// NTP settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;      // UTC
const int daylightOffset_sec = 0;  // No DST

// Serial connection to Arduino Uno
#define ARDUINO_RX 16 // Connect to Uno TX (pin 1)
#define ARDUINO_TX 17 // Connect to Uno RX (pin 0)
HardwareSerial& arduinoSerial = Serial2;

// Timing constants
const long fetchInterval = 15000; // 15 seconds
unsigned long lastFetchTime = 0;

// State management
bool waitingForValidation = false;

void setup() {
  Serial.begin(115200);
  arduinoSerial.begin(9600, SERIAL_8N1, ARDUINO_RX, ARDUINO_TX);

  Serial.println("ESP32 Starting...");

  connectWiFi();
  syncTime();

  // Initial fetch
  fetchAndSendParcels();
}

void loop() {
  // Reconnect if WiFi is lost
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Check for Arduino delivery confirmation
  if (arduinoSerial.available()) {
    String json = arduinoSerial.readStringUntil('\n');
    Serial.print("Received from Arduino: ");
    Serial.println(json);
    updateParcelStatus(json);
  }

  // Only poll for new OTPs if not waiting for validation
  if (!waitingForValidation) {
    if (millis() - lastFetchTime > fetchInterval) {
      Serial.println("Checking for new OTP...");
      fetchAndSendParcels();
      lastFetchTime = millis(); // Reset timer
    }
  }
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Syncing time with NTP server...");
  int attempts = 0;
  while (!time(nullptr) && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  if (attempts < 10) {
    Serial.println("\nTime synchronized!");
    printLocalTime();
  } else {
    Serial.println("\nTime sync failed!");
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.println("Current Local Time: " + String(timeString));
}

void fetchAndSendParcels() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return;
  }

  HTTPClient http;
  http.begin(supabaseUrl);
  http.addHeader("apikey", apiKey);
  http.addHeader("Content-Type", "application/json");

  int retries = 0;
  const int maxRetries = 3;
  while (retries <= maxRetries) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      if (payload.length() == 0) {
        Serial.println("Empty response from Supabase. No pending parcels.");
        break;
      }

      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        retries++;
        delay(5000);
        continue;
      }

      if (doc.is<JsonArray>() && doc.size() > 0) {
        JsonObject parcel = doc[0].as<JsonObject>();
        int id = parcel["id"].as<int>();
        int otp_int = parcel["otp"].as<int>();

        if (id > 0 && otp_int >= 0) { // Allow OTP = 0
          String data = String(id) + "," + String(otp_int);
          arduinoSerial.println(data);
          Serial.println("Sent to Arduino: " + data);
          waitingForValidation = true; // Enter validation mode
        } else {
          Serial.println("No valid OTP found. Will check again in 15 seconds.");
        }
      } else {
        Serial.println("No pending deliveries found");
      }
      break;
    } else {
      Serial.print("HTTP GET failed (code: ");
      Serial.print(httpCode);
      Serial.println("). Retrying...");
      retries++;
      delay(5000);
    }
  }
  http.end();
}

String getCurrentISOTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Unknown";
  }
  char isoTime[20];
  strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(isoTime);
}

void updateParcelStatus(String jsonData) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  int id = doc["id"].as<int>();
  const char* status = doc["status"];

  String endpoint = String("https://ylmulnevakkhmoocxnhk.supabase.co/rest/v1/deliveries?id=eq.") + String(id);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected!");
    return;
  }

  HTTPClient http;
  http.begin(endpoint);
  http.addHeader("apikey", apiKey);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  StaticJsonDocument<200> updateDoc;
  updateDoc["delivery_status"] = status;
  updateDoc["delivered_time"] = getCurrentISOTime();

  String body;
  serializeJson(updateDoc, body);
  Serial.println("Sending PATCH request to: " + endpoint);
  Serial.println("Body: " + body);

  int retries = 0;
  const int maxRetries = 3;
  while (retries <= maxRetries) {
    int httpCode = http.PATCH(body);
    if (httpCode == 204) {
      Serial.println("Parcel status updated successfully.");
      waitingForValidation = false; // Reset state
      fetchAndSendParcels(); // Check for new OTP immediately
      break;
    } else {
      Serial.print("PATCH failed (code: ");
      Serial.print(httpCode);
      Serial.println("). Retrying...");
      retries++;
      delay(5000);
    }
  }
  http.end();
}