//Final Arduino code: // Sensors Used //
// 1. DS18B20 Waterproof Temperature Sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

SoftwareSerial mySerial(10, 11); // RX, TX
/////////////////////
// 2. Ultrasonic Sensor
#define trigPin 2
#define echoPin 4
#define D_MIN 2 // Minimum distance reading when the tank is full, adjust based on your measurements


// Pump H-Bridge
// Define pins for Pump 1
const int pump1_in1 = 6;
const int pump1_in2 = 7;

// Define pins for Pump 2
const int pump2_in1 = 8;
const int pump2_in2 = 9;

// LEDs for testing
//#define redLedMain 7
#define blueLedSupply 13

#define btn 5 // Pump2 Button


/////////////////////////////////////////
// DS18B20 Digital Temperature Sensor //
////////////////////////////////////////

// Data wire is connected to Arduino pin 3
#define ONE_WIRE_BUS 3

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

//////////////////////////
// 3. Turbidity Sensor //
////////////////////////
#define turbPin A0 

bool isPumpOn = false; // Track the state of the pump

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //Setup H-bridge pumps
  pinMode(pump1_in1, OUTPUT);
  pinMode(pump1_in2, OUTPUT);
  pinMode(pump2_in1, OUTPUT);
  pinMode(pump2_in2, OUTPUT);
  
  // Initially turn off both pumps
  stopPump1();
  stopPump2();
  isPumpOn = false;

  pinMode(btn, INPUT);
  pinMode(blueLedSupply, OUTPUT);

  sensors.begin();  // Start up the library
}

void loop() {

  //outer pump state
  if(isPumpOn){
    Serial.println("Outer pumpstate is true");
  } else{
    Serial.println("Outer pumpstate is false");
  }
  // 2. Temperature Measurement
  float temp = fetchTemperature();
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" °C");

  // 1. Water Quantity
  long waterQty = calcWaterHeight();
  Serial.print("Water cm:");
  Serial.println(waterQty);

   
  if(waterQty <= 5) {
     if(waterQty < 13){
       startPump1();
     }
  } else {
     Serial.println("Pump is OFF");
     stopPump1();
  }

  // 3. Turbidity
  float ntu = fetchTurbidity();

  // Receive the pump state fetched from ThingSpeak by ESP32 and process it
  if (mySerial.available() > 0) {
    String receivedData = mySerial.readStringUntil('\n');
    Serial.print("Received pump state from mern: ");
    Serial.println(receivedData);

    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, receivedData);
    
    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.f_str());
      return;
    }

    int pumpState = doc["state"];
    if (pumpState == 1) {
      if (isPumpOn) {
        Serial.println("Pump2 is already on");
      } else {
        isPumpOn = true;
        startPump2();
        Serial.println("Pump2 ON via ESP32");
      }
    } else if (pumpState == 0) {
      if (!isPumpOn) {
        Serial.println("Pump2 is already off");
      } else {    
        stopPump2();
        Serial.println("Pump2 OFF via ESP32");
        isPumpOn = false;
      }
    } 
  }


 /// PUMP 2 
  int pumpBtn =  digitalRead(btn); // Read the button state

  // Check if the button was pressed (transition from HIGH to LOW)
  if (pumpBtn) {
      Serial.println("Button pressed");
      startPump2();
      isPumpOn = true;
      Serial.println("Pump2 ON via Button");
    } else {
      stopPump2();
      isPumpOn = false;
      Serial.println("Pump2 OFF via Button");
    }
  // Send data to ESP32
  mySerial.print(temp); // Temperature
  mySerial.print(",");
  mySerial.print(waterQty); // Water level
  mySerial.print(",");
  mySerial.print(isPumpOn ? "1" : "0"); // Pump state: ON | OFF
  mySerial.print(",");
  mySerial.print(ntu);
  mySerial.print(";"); 

  delay(1000);
}

//pump functions

void startPump1() {
  digitalWrite(pump1_in1, HIGH);
  digitalWrite(pump1_in2, LOW);
}

void stopPump1() {
  digitalWrite(pump1_in1, LOW);
  digitalWrite(pump1_in2, LOW);
}

void startPump2() {
  digitalWrite(pump2_in1, HIGH);
  digitalWrite(pump2_in2, LOW);
   digitalWrite(blueLedSupply, HIGH);
}

void stopPump2() {
  digitalWrite(pump2_in1, LOW);
  digitalWrite(pump2_in2, LOW);
   digitalWrite(blueLedSupply, LOW);
}


//calculate water level
long calcWaterHeight() {
  long distance, duration;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration / 2) / 29.1;

  long waterQty = 20 - distance;

  // Calculate the water level percentage
  // float waterLevel = (1 - (distance - D_MIN) / (20.0 - D_MIN)) * 100;
  // Constrain percentage between 0% and 100%
 // waterLevelPercentage = constrain(waterLevelPercentage, 0, 100);

  return waterQty;
}


//calculate temperature
float fetchTemperature() {
  // Request temperature measurement
  sensors.requestTemperatures();
  // Fetch temperature in Celsius
  float temperatureC = sensors.getTempCByIndex(0);
  return temperatureC;
}


//calculate turbidity
float fetchTurbidity(){
  float volt, ntu;

  // The output analog voltage from the sensor has huge variations and is too noisy to measure. 
  // Hence we took 800 readings and then took the average value for reading.
  volt = 0;
  for(int i = 0; i < 800; i++) {
    volt += ((float)analogRead(turbPin) / 1023) * 5;
  }
  volt = volt / 800;
  volt = round_to_dp(volt, 2);
  // Turbidity range from 2.5 V to 4.2 V (3,000 to 0 turbidity)
  if (volt < 2.5) {
    ntu = 3000;
  } else {
    ntu = -1120.4* (volt*volt) + 5742.3 * volt - 4353.8; // Convert the analog voltage value to NTU
  }

  Serial.print(volt);
  Serial.println(" V");
  Serial.print(ntu);
  Serial.println(" NTU");

  return ntu;
}

float round_to_dp(float in_value, int decimal_place) {
  float multiplier = powf(10.0f, decimal_place);
  in_value = roundf(in_value * multiplier) / multiplier;
  return in_value;
}