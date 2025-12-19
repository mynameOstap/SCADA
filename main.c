#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>

const char* AP_SSID = "ESP8266_LAB";
const char* AP_PASS = "12345678";  

const char* MQTT_HOST = "192.168.4.2"; 
const uint16_t MQTT_PORT = 1883;

const char* MQTT_USER = "";  
const char* MQTT_PASS = "";

const char* MQTT_TOPIC_DATA   = "esp32/data";
const char* MQTT_TOPIC_STATUS = "esp32/status";

const char* DEVICE_ID = "esp8266-12f-01";

const int SERVO_PIN  = 14;  
const int SENSOR_PIN = A0;

Servo myServo;
static bool openValve = false;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastPublishMs = 0;
const unsigned long PUBLISH_EVERY_MS = 2000;

void startAP() {
  WiFi.mode(WIFI_AP);

  IPAddress apIP(192,168,4,1);
  IPAddress netM(255,255,255,0);
  WiFi.softAPConfig(apIP, apIP, netM);

  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("AP started: ");
  Serial.println(AP_SSID);
  Serial.print("ESP AP IP: ");
  Serial.println(WiFi.softAPIP()); 
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  while (!mqtt.connected()) {
    Serial.print("MQTT connecting... ");

    bool ok;
    if (strlen(MQTT_USER) > 0) {
      ok = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS,
                        MQTT_TOPIC_STATUS, 0, true, "offline");
    } else {
      ok = mqtt.connect(DEVICE_ID,
                        MQTT_TOPIC_STATUS, 0, true, "offline");
    }

    if (ok) {
      Serial.println("OK");
      mqtt.publish(MQTT_TOPIC_STATUS, "online", true);
    } else {
      Serial.print("FAIL rc=");
      Serial.print(mqtt.state());
      Serial.println(" retry 2s");
      delay(2000);
    }
  }
}

void publishData(int raw, int percent, bool valveOpen) {
  char payload[180];
  snprintf(payload, sizeof(payload),
           "{\"device\":\"%s\",\"waterRaw\":%d,\"waterPercent\":%d,\"valveOpen\":%s}",
           DEVICE_ID, raw, percent, valveOpen ? "true" : "false");

  mqtt.publish(MQTT_TOPIC_DATA, payload, false);
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  startAP();     
  connectMQTT();  
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  int raw = analogRead(SENSOR_PIN);
  int percent = (raw * 100) / 1023;

  if (percent >= 80 && !openValve) { myServo.write(90); openValve = true; }
  if (percent <= 20 && openValve)  { myServo.write(0);  openValve = false; }

  unsigned long now = millis();
  if (now - lastPublishMs >= PUBLISH_EVERY_MS) {
    lastPublishMs = now;
    publishData(raw, percent, openValve);
  }

  delay(50);
}
