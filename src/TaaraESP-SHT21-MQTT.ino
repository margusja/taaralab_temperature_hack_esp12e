/*
 * Send temperature and humidity data from SHT21/HTU21 sensor on ESP8266 to EmonCMS over HTTPS or HTTP
 *
 * Demo for TaaraESP SHT21 Wifi Humidity sensor https://taaralabs.eu/es1
 *
 * by Märt Maiste, TaaraLabs
 * https://github.com/TaaraLabs/TaaraESP-SHT21-EmonCMS
 * 2016-08-08
 *
*/

#include <ESP8266WiFi.h>       // https://github.com/esp8266/Arduino
#include <WiFiClientSecure.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager

#include "SparkFunHTU21D.h"    // https://github.com/sparkfun/SparkFun_HTU21D_Breakout_Arduino_Library

#include <PubSubClient.h>      // https://github.com/knolleary/pubsubclient

#include <EEPROM.h>

#define DEBUG 1          // serial port debugging
//#define HTTPS 0          // use secure connection

// Set web server port number to 80
ESP8266WebServer server(80);

uint addr = 0;

// fake data
struct {
  uint mqtt_port = 0;
  uint idx = 0;
  char mqtt_server[20] = "";
  uint config = 0;
} data;

String temp_str;
char temp[80];

char hostname[] = "Temperatuuriandur-3";

const int interval  = 5; // send readings every 5 minutes

const char* ssid = "varmar";
const char* password = "xxx";

WiFiClient espClient;
PubSubClient client(espClient);

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266! \n\n http://IP/set?mqtt_server=[MQTT SERVER]&mqtt_port=[MQTT port]&idx=[IDX]");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    server.handleClient(); // give a chance to set mqtt server
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("tele/esp8266_sensor_test/temp", "23");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into
  // a 512-byte-array cache in RAM
  EEPROM.begin(512);

  // read bytes (i.e. sizeof(data) from "EEPROM"),
  // in reality, reads from byte-array cache
  // cast bytes into structure called data
  EEPROM.get(addr,data);

  Serial.println("\n\nTaaraESP SHT21 [taaralabs.eu/es1]\n");

  HTU21D sht;
  Wire.begin(2,14);        // overriding the i2c pins
  sht.begin();           // default: sda=4, scl=5
  float humidity    = sht.readHumidity();
  float temperature = sht.readTemperature();

  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("Humidity: ");
  Serial.println(humidity);

  Serial.print("ESP chip ID: ");
  Serial.println(ESP.getChipId());

 // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  wifi_station_set_hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();

  server.on("/", handleRoot);

  server.on("/set", []() {
    Serial.println(server.arg(0));
    Serial.println(server.arg(1));
    Serial.println(server.arg(2));
    server.arg(0).toCharArray(data.mqtt_server, 20);
    data.mqtt_port = server.arg(1).toInt();
    data.idx = server.arg(2).toInt();
    data.config = true;
    EEPROM.put(addr,data);
    EEPROM.commit();

    server.send(200, "text/plain", "Done. Going to restart ESP module");

    delay(100);
    ESP.reset();
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");


  client.setServer(data.mqtt_server, data.mqtt_port);

  if (!client.connected()) {
    reconnect();
  }

  Serial.println("Publishing data");

  // This sends off your payload.
  // This sends off your payload.
  temp_str = "{\"idx\":"+ String(data.idx) +",\"nvalue\":0,\"svalue\": \"" + String(temperature) + "\",\"Battery\":48,\"RSSI\":5}"; //converting ftemp (the float variable above) to a string
  temp_str.toCharArray(temp, temp_str.length() + 1); //packaging up the data to publish to mqtt whoa..

  // try to publish and if it failes, blink 2 times, wait a second and repeat for 5 minutes
  if (!client.publish("domoticz/in", temp)) {
    Serial.println("Publishing failed");

    ESP.reset();
    delay(5000);
  }

  Serial.print("Going to sleep for ");
  Serial.print(interval);
  Serial.println(" minutes");
  delay(3000);                            // let the module to get everything in order in the background
  ESP.deepSleep(interval * 60 * 1000000); // deep sleep means turning off everything except the wakeup timer. wakeup call resets the chip
  delay(5000);                            // going to sleep may take some time
}

void loop() {                             // we should never reach this point, but if we do, then
  delay(3000);
  ESP.reset();                            // and reset the module
  delay(5000);
}
