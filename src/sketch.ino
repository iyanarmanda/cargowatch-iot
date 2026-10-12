#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "secrets.h"

#define DHTPIN 15
#define DHTTYPE DHT22
#define LED_PIN 2
#define MPU_ADDR 0x68

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

String deviceId;
String mqttTopic;
unsigned long prevBlinkMillis = 0;
unsigned long prevSendMillis = 0;
bool alertState = false;

String getUniqueDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();
  char id[20];
  snprintf(id, sizeof(id), "ESP32-%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(id);
}

void handleAlert(bool isAlert) {
  if (isAlert) {
    if (millis() - prevBlinkMillis >= 150) {
      prevBlinkMillis = millis();
      alertState = !alertState;
      digitalWrite(LED_PIN, alertState ? HIGH : LOW);
    }
  } else {
    alertState = false;
    digitalWrite(LED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  dht.begin();
  
  Wire.begin(21, 22);
  Wire.setClock(100000);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" WiFi Failed! Running in Offline Mode.");
  }

  deviceId = getUniqueDeviceId();
  mqttTopic = "v1/telemetry/" + deviceId + "/data";
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setSocketTimeout(1);
}

void maintainMQTTConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      Serial.print("Connecting to MQTT Broker...");
      if (mqttClient.connect(deviceId.c_str(), MQTT_USER, MQTT_PASS)) {
        Serial.println(" Connected!");
      } else {
        Serial.print(" Failed, rc=");
        Serial.println(mqttClient.state());
      }
    } else {
      mqttClient.loop();
    }
  }
}

void loop() {
  float temp = dht.readTemperature();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();

  float ax_g = ax / 16384.0;
  float ay_g = ay / 16384.0;
  float az_g = az / 16384.0;
  float gForce = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

  bool tempAlert = (temp > 8.0 || temp < 2.0);
  bool shockAlert = (gForce > 2.5);
  bool isAlert = (tempAlert || shockAlert);

  handleAlert(isAlert);

  if (millis() - prevSendMillis >= 2000) {
    prevSendMillis = millis();

    maintainMQTTConnection();

    if (mqttClient.connected()) {
      JsonDocument doc;
      doc["device_id"] = deviceId;
      doc["temperature"] = serialized(String(temp, 1));
      doc["g_force"] = serialized(String(gForce, 2));
      doc["temp_alert"] = tempAlert;
      doc["shock_alert"] = shockAlert;

      char jsonBuffer[200];
      serializeJson(doc, jsonBuffer);

      mqttClient.publish(mqttTopic.c_str(), jsonBuffer);
      Serial.print("-> MQTT Published: ");
      Serial.println(jsonBuffer);
    }
  }

  delay(200);
}
