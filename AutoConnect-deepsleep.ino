#include <FS.h>                   
#include <Wire.h>
#include "Adafruit_MCP9808.h"

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>          

#include <BlynkSimpleEsp8266.h>
#include <PubSubClient.h>

#define TRIGGER_PIN 13 //the pushbutton

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

int start = 0;
bool isConfig = false;
bool shouldSaveConfig = false;
float lastTemp = 0.0;

char mqtt_server[40];
char mqtt_port[6] = "8080";
char mqtt_user[40];
char mqtt_password[40];
char mqtt_topic[40];
char blynk_token[34] = "YOUR_BLYNK_TOKEN";

void doMeasurement() {
  while (!tempsensor.begin()) {
    Serial.println("Couldn't find MCP9808!");
    delay(100);
  }
  
  Serial.println("\nwake up MCP9808.... "); 
  tempsensor.wake();   // wake up, ready to read!
  lastTemp = tempsensor.readTempC();
  Serial.print("Temp: "); Serial.print(lastTemp); Serial.println("*C\t"); 
  Serial.println("Shutdown MCP9808.... ");
  tempsensor.shutdown();
}

void sendBlynk() {
  Serial.println("Connecting to Blynk");
  Blynk.config(blynk_token);
  Blynk.connect();
  Serial.println("Sending data to Blynk");
  Blynk.virtualWrite(V5, lastTemp);
}

void sendMqtt() {
  WiFiClient espClient;
  PubSubClient client(espClient);
  client.setServer(mqtt_server, atoi(mqtt_port));
  Serial.print("Attempting MQTT connection...");
  String clientId = "Client-" + String();

  String payload = String(ESP.getChipId());
  payload.concat(" ");
  payload.concat(lastTemp);

  if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
     Serial.println("connected");
     client.publish(mqtt_topic, payload.c_str(), true);
     client.disconnect();
  } else {
     Serial.print("failed, rc=");
     Serial.print(client.state());
  }

}


void reloadConfiguration() {
    Serial.println("mounting FS...");
    if (SPIFFS.begin()) {
      Serial.println("mounted file system...");
      if (SPIFFS.exists("/config.json")) {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);
  
          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(buf.get());
          json.printTo(Serial);
          if (json.success()) {
            Serial.println("\nparsed json");
  
            strcpy(mqtt_server, json["mqtt_server"]);
            strcpy(mqtt_port, json["mqtt_port"]);
            strcpy(mqtt_user, json["mqtt_user"]);
            strcpy(mqtt_password, json["mqtt_password"]);
            strcpy(mqtt_topic, json["mqtt_topic"]);
            
            strcpy(blynk_token, json["blynk_token"]);
  
          } else {
            Serial.println("failed to load json config");
          }
        }
      }
    } else {
      Serial.println("failed to mount FS");
    }
}


void establishWiFi() {
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect();
  Serial.println("connected...yeey :)");
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configPortal() {
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
    WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 40);
    WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 40);
    WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);
   
    WiFiManager wifiManager;
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_password);
    wifiManager.addParameter(&custom_mqtt_topic);
    wifiManager.addParameter(&custom_blynk_token);

    String ssid = "ESP" + String(ESP.getChipId());

    if (!wifiManager.startConfigPortal(ssid.c_str())) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    Serial.println("connected...yeey :)");

    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_password, custom_mqtt_password.getValue());
    strcpy(mqtt_topic, custom_mqtt_topic.getValue());
    strcpy(blynk_token, custom_blynk_token.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["mqtt_topic"] = mqtt_topic;
    json["blynk_token"] = blynk_token;
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
}


void setup() {
  Serial.begin(9600);
  pinMode(TRIGGER_PIN, INPUT);
  start = millis();

  if (digitalRead(TRIGGER_PIN) == HIGH) {
        Serial.println("Starting in configuration mode");
        isConfig = true;
  }

  reloadConfiguration();

  if (!isConfig) {
    establishWiFi();
    doMeasurement();
    sendBlynk();
    sendMqtt();
    Serial.printf("Cycle time: %d ms\n", millis() - start);
    Serial.println("Sleeping for 60 sec...");
    ESP.deepSleep(60 * 1000000);
  } else {
    configPortal();
    ESP.restart();
  }
}

void loop() {
  //do nothing in the loop
}
