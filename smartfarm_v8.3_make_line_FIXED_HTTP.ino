#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <RTClib.h>

// ==================== MQTT ====================
const char* mqtt_server = "650188a0ee2b4367b7c131fb385590a9.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "smartfarm";
const char* mqtt_pass = "Kla12345";

// ==================== Relay ====================
#define RELAY_PIN D5

// ==================== Schedule ====================
String schedule1On  = "06:00";
String schedule1Off = "06:10";
String schedule2On  = "17:00";
String schedule2Off = "17:10";
String lastScheduleAction = "";

// ==================== RTC ====================
RTC_DS3231 rtc;

// ==================== MQTT Client ====================
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastRTC = 0;
unsigned long lastScheduleCheck = 0;

void sendMake(String msg)
{
  WiFiClientSecure clientSecure;
  clientSecure.setInsecure();
  HTTPClient http;

  Serial.println("Connecting Make...");

  if (!http.begin(clientSecure, "https://hook.eu1.make.com/38v3gycrn587f16g628b1rkulacivaex"))
  {
    Serial.println("HTTP Begin Failed");
    return;
  }

  http.addHeader("Content-Type","application/json");
  String payload="{\"message\":\"" + msg + "\"}";
  int code=http.POST(payload);

  Serial.print("HTTP Code: ");
  Serial.println(code);

  Serial.println(http.getString());
  http.end();
}


void publishStatus()
{
  if (digitalRead(RELAY_PIN) == LOW)
    client.publish("farm/pump/status", "ON", true);
  else
    client.publish("farm/pump/status", "OFF", true);
}

void publishTime()
{
  DateTime now = rtc.now();
  char timeString[20];

  sprintf(timeString,"%02d:%02d:%02d",
          now.hour(),now.minute(),now.second());

  client.publish("farm/time", timeString, true);
}

void publishSchedules()
{
  client.publish("farm/schedule1/on/status", schedule1On.c_str(), true);
  client.publish("farm/schedule1/off/status", schedule1Off.c_str(), true);
  client.publish("farm/schedule2/on/status", schedule2On.c_str(), true);
  client.publish("farm/schedule2/off/status", schedule2Off.c_str(), true);
}

void checkSchedule()
{
  DateTime now = rtc.now();

  char currentTime[6];
  sprintf(currentTime,"%02d:%02d",now.hour(),now.minute());

  String timeNow = String(currentTime);

  if (timeNow == schedule1On && lastScheduleAction != "S1ON")
  {
    digitalWrite(RELAY_PIN, LOW);
    publishStatus();
    sendMake("🌱 เริ่มรดน้ำรอบเช้า");
    lastScheduleAction = "S1ON";
  }
  else if (timeNow == schedule1Off && lastScheduleAction != "S1OFF")
  {
    digitalWrite(RELAY_PIN, HIGH);
    publishStatus();
    sendMake("✅ จบรดน้ำรอบเช้า");
    lastScheduleAction = "S1OFF";
  }
  else if (timeNow == schedule2On && lastScheduleAction != "S2ON")
  {
    digitalWrite(RELAY_PIN, LOW);
    publishStatus();
    sendMake("🌙 เริ่มรดน้ำรอบเย็น");
    lastScheduleAction = "S2ON";
  }
  else if (timeNow == schedule2Off && lastScheduleAction != "S2OFF")
  {
    digitalWrite(RELAY_PIN, HIGH);
    publishStatus();
    sendMake("✅ จบรดน้ำรอบเย็น");
    lastScheduleAction = "S2OFF";
  }

  if (now.hour() == 0 && now.minute() == 0)
    lastScheduleAction = "";
}

void callback(char* topic, byte* payload, unsigned int length)
{
  String msg = "";

  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  String topicStr = String(topic);

  if (topicStr == "farm/pump")
  {
    if (msg == "ON")
    {
      digitalWrite(RELAY_PIN, LOW);
      publishStatus();
    }
    else if (msg == "OFF")
    {
      digitalWrite(RELAY_PIN, HIGH);
      publishStatus();
    }
  }

  else if (topicStr == "farm/schedule1/on") schedule1On = msg;
  else if (topicStr == "farm/schedule1/off") schedule1Off = msg;
  else if (topicStr == "farm/schedule2/on") schedule2On = msg;
  else if (topicStr == "farm/schedule2/off") schedule2Off = msg;

  publishSchedules();
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Connecting MQTT...");

    if (client.connect("ESP8266Farm", mqtt_user, mqtt_pass))
    {
      Serial.println("MQTT Connected");
      client.subscribe("farm/pump");
      client.subscribe("farm/schedule1/on");
      client.subscribe("farm/schedule1/off");
      client.subscribe("farm/schedule2/on");
      client.subscribe("farm/schedule2/off");

      client.publish("farm/device/status", "ONLINE", true);

      publishStatus();
      publishSchedules();
      sendMake("📡 Smart Farm Online");
    }
    else
    {
      Serial.print("MQTT Failed rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("===== SMART FARM =====");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  WiFiManager wm;

  if (!wm.autoConnect("SmartFarm_Setup"))
    ESP.restart();

  Wire.begin(D2, D1);

  rtc.begin();

  if (rtc.lostPower())
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  espClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.reconnect();
    return;
  }

  if (!client.connected())
    reconnect();

  client.loop();

  if (millis() - lastRTC > 5000)
  {
    publishTime();
    lastRTC = millis();
  }

  if (millis() - lastScheduleCheck > 1000)
  {
    checkSchedule();
    lastScheduleCheck = millis();
  }
}
