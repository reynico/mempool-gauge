#include <Servo.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>

#define TRIGGER_PIN 2

Servo s1;

unsigned long prev_millis = 0;
const long check_interval = 60000;  // 1 minute in milliseconds

void setup() {
  s1.attach(15, 370, 2400);  // GPIO15 / D8
  set_servo(0); // start inverted
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  Serial.begin(115200);

  for (int i = 0; i <= 100; i++) {
    reset_default();
    delay(100);
  }

  WiFiManager wifiManager;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi, starting Config Portal");
    wifiManager.autoConnect("mempool", "notsatoshi");
  }
  Serial.printf("Connected to WiFi: %s with IP: %s\n", wifiManager.getWiFiSSID().c_str(), WiFi.localIP().toString().c_str());
  fetch_fees();
}

void reset_default() {
  if (digitalRead(TRIGGER_PIN) == LOW) {
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW) {
      WiFiManager wifiManager;
      delay(3000);
      if (digitalRead(TRIGGER_PIN) == LOW) {
        Serial.println("Reset!");
        wifiManager.resetSettings();
        ESP.restart();
      }
      wifiManager.setConfigPortalTimeout(120);
      if (!wifiManager.startConfigPortal("mempool", "notsatoshi")) {
        Serial.println("Failed to connect or hit timeout");
        delay(3000);
      } else {
        Serial.println("Connected (again)");
      }
    }
  }
}

void loop() {
  unsigned long current_millis = millis();
  if (current_millis - prev_millis >= check_interval) {
    prev_millis = current_millis;
    fetch_fees();
  }
}

void fetch_fees() {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  if (https.begin(*client, "https://www.mempool.space/api/v1/fees/recommended")) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println(payload);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.printf("Couldn't deserialize json: %s\n", error.c_str());
          https.end();
          return;
        }
        int fastest_fee = doc["fastestFee"];
        set_servo(fastest_fee);
      }
    } else {
      Serial.printf("Couldn't make GET request: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println("Unable to connect");
  }
}


void set_servo(int fee) {
  if (fee > 90) {
    fee = 90;
  } else if (fee < 5) {
    fee = 5;
  }

  int angle = map(fee, 1, 90, 180, 0);
  s1.write(angle);
}
