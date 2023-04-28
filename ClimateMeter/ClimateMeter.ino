#include "DHT.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "arduino_secrets.h"

// WiFi
char *ssid = SECRET_SSID;        // your network SSID (name)
char *pass = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)

// MQTT
const char *mqtt_broker = MQTT_IP;
const char *mqtt_username = MQTT_USER;
const char *mqtt_password = MQTT_PASS;
const int   mqtt_port = MQTT_PORT;
const char *topic_temperature  = "home/office/esp8266/temperature";
const char *topic_humidity  = "home/office/esp8266/humidity";
const char *topic_status  = "home/office/esp8266/status";
const char *topic_updatetime  = "home/office/esp8266/updatetime";

// NTP
const char *ntp_addr = "pool.ntp.org";
int previousDaylightSaving = -32768;

// Set interval for sending messages (milliseconds)
const long interval = 60000;
unsigned long previousMillis = 0;

DHT dht;
WiFiClient wclient;
WiFiUDP ntpUDP;
PubSubClient client(wclient);
NTPClient timeClient(ntpUDP, ntp_addr);

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}

void setup() {
  // Serial setup
  Serial.begin(9600);
  while (!Serial) { ; } // wait for serial port to connect. Needed for native USB port only

  // DHT11 setup
  dht.setup(D1);

  // WiFi setup
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
      delay(5000);
      Serial.println("...");
  }

  Serial.println("Connected to the network");
  Serial.println();

  // MQTT setup
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
      String client_id = "esp8266-client-";
      client_id += String(WiFi.macAddress());
      Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
      if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      } else {
          Serial.print("failed with state ");
          Serial.print(client.state());
          delay(2000);
      }
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  // NTPClient setup
  timeClient.begin();

  // Set GMT+2
  timeClient.setTimeOffset(7200);
}

void loop() {
  // Keep connection to MQTT server
  client.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= /*dht.getMinimumSamplingPeriod() + */interval || previousMillis == 0) {
    previousMillis = currentMillis;

    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();
    const char *status = dht.getStatusString();
    Serial.println(status);
    
    if (status == "OK") {
      char temperature_result[8];
      dtostrf(temperature, 6, 2, temperature_result);

      char humidity_result[8];
      dtostrf(humidity, 6, 2, humidity_result);

      Serial.println(temperature_result);
      Serial.println(humidity_result);

      // Update NTP
      timeClient.update();

      // HH:MM:SS
      String formattedTime = timeClient.getFormattedTime();

      unsigned long epochTime = timeClient.getEpochTime();
      struct tm *ptm = gmtime((time_t *)&epochTime);

      int day = ptm->tm_mday;
      int month = ptm->tm_mon + 1;
      int year = ptm->tm_year + 1900;

      String fixedDay = "";
      String fixedMonth = "";

      if (day < 10) {
        fixedDay = String("0") + String(day);
      }
      else {
        fixedDay = String(day);
      }

      if (month < 10) {
        fixedMonth = String("0") + String(month);
      }
      else {
        fixedMonth = String(month);
      }

      String currentDate = String(year) + "-" + fixedMonth + "-" + fixedDay;
      String dt = currentDate + "T" + formattedTime;

      Serial.println(dt);

      client.publish(topic_temperature, temperature_result);
      client.publish(topic_humidity, humidity_result);
      client.publish(topic_updatetime, dt.c_str());
    }

    client.publish(topic_status, status);
  }
}