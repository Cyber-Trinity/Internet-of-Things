#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Diego Lopez";
const char* password = "Diego@Lopez";

// MQTT Broker settings
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;

// Create a WiFiClient and PubSubClient object
WiFiClient espClient;
PubSubClient client(espClient);

// MQTT Topics
const char* publishTopic = "waterwatch/data";
const char* subscribeTopic = "waterwatch/pumpstate";

// Pins for Serial communication (adjust if necessary)
const int rxPin = 16;
const int txPin = 17;

// Function to handle MQTT messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (String(topic) == subscribeTopic) {
    // Parse the received pump state message
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.f_str());
      return;
    }
    bool pumpState = doc["state"].as<int>();
    Serial2.print("{\"state\":"); 
    Serial2.print(pumpState);
    Serial2.println("}");
    Serial.print("Pump state received from app: ");
    Serial.println(pumpState);
  }
}

// Function to connect to the MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection… ");
    String clientId = "ESP32Client_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected!");
      client.subscribe(subscribeTopic);
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000); // Increase delay for better recovery
    }
  }
}

// Function to set up the WiFi connection
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, rxPin, txPin); // Initialize Serial2 with RX and TX pins

  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read from Serial2
  if (Serial2.available()) {
    String data = Serial2.readStringUntil(';'); // Read until semicolon

    // Parse data with error handling
    int commaIndex1 = data.indexOf(',');
    int commaIndex2 = data.indexOf(',', commaIndex1 + 1);
    int commaIndex3 = data.indexOf(',', commaIndex2 + 1);

    if (commaIndex1 != -1 && commaIndex2 != -1 && commaIndex3 != -1) {
      float temp = data.substring(0, commaIndex1).toFloat();
      float waterQty = data.substring(commaIndex1 + 1, commaIndex2).toFloat();
      String isPumpOn = data.substring(commaIndex2 + 1, commaIndex3);
      float ntu = data.substring(commaIndex3 + 1).toFloat();

      Serial.print("temp: "); Serial.println(temp);
      Serial.print("waterQty: "); Serial.println(waterQty);
      Serial.print("pumpState from arduino: "); Serial.println(isPumpOn);
      Serial.print("turbidity: "); Serial.println(ntu);

      // Publish data to MQTT
      DynamicJsonDocument doc(1024);
      doc["waterLevel"] = waterQty;
      doc["temp"] = temp;
      doc["turbidity"] = ntu;
      doc["pumpState"] = isPumpOn.toInt() == 1; // Convert to boolean

      String payload;
      serializeJson(doc, payload);
      client.publish(publishTopic, payload.c_str());
    } else {
      Serial.println("Error parsing data: Invalid format");
    }
  }

  delay(1000);  // Delay 1 second
}