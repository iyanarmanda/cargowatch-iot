#include <Wire.h>
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22
#define LED_PIN 2
#define MPU_ADDR 0x68

DHT dht(DHTPIN, DHTTYPE);

unsigned long prevBlinkMillis = 0;
bool alertState = false;

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
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
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

  Serial.print("Temperature: ");
  Serial.print(temp, 1);
  Serial.print(" C | G-Force: ");
  Serial.print(gForce, 2);
  Serial.print(" G | Status: ");

  if (tempAlert && shockAlert) {
    Serial.println("TEMP & SHOCK ALERT");
  } else if (tempAlert) {
    Serial.println("TEMP ALERT");
  } else if (shockAlert) {
    Serial.println("SHOCK ALERT");
  } else {
    Serial.println("NORMAL");
  }

  delay(200);
}
