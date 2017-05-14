#include <FS.h>                   
#include <ESP8266WiFi.h>          

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>          

#include <Wire.h>
#include "Adafruit_MCP9808.h"
#include <BlynkSimpleEsp8266.h>
#include <PubSubClient.h>


#define TRIGGER_PIN 13

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

char mqtt_server[40];
char mqtt_port[6] = "8080";
char mqtt_user[40];
char mqtt_password[40];
char mqtt_topic[40];
char blynk_token[34] = "YOUR_BLYNK_TOKEN";

bool shouldSaveConfig = false;
BlynkTimer timer;

WiFiClient espClient;
PubSubClient client(espClient);

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void myTimerEvent()
{
  float c = tempsensor.readTempC();
  Blynk.virtualWrite(V5, c);
  Blynk.virtualWrite(V6, millis() / 1000);
  Serial.print("Temp: "); Serial.print(c); Serial.println("*C\t"); 

  client.publish(mqtt_topic, String(c).c_str(), true);
}

void setup() {
    Serial.begin(115200);
    pinMode(TRIGGER_PIN, INPUT);

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

    WiFiManager wifiManager;
    wifiManager.autoConnect();

    Serial.println("local ip");
    Serial.println(WiFi.localIP());

  if (!tempsensor.begin()) {
    Serial.println("Couldn't find MCP9808!");
    while (1);
  }

  Blynk.config(blynk_token);
  Blynk.connect();
  timer.setInterval(60000L, myTimerEvent);
  client.setServer(mqtt_server, atoi(mqtt_port));
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

  Blynk.config(blynk_token);
  Blynk.connect();
}


void reconnect() {
  // Loop until we're reconnected
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("temp", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(1000);
    }
  }
}

void loop() {
  Blynk.run();
  timer.run(); // Initiates BlynkTimer


  if (digitalRead(TRIGGER_PIN) == LOW) {
        Serial.println("Starting config portal...");
        configPortal();
  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
}
