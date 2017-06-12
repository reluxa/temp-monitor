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
char captive_user[40];
char captive_password[40];

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
          //json.printTo(Serial);
          if (json.success()) {
            Serial.println("\nparsed json");
  
            strcpy(mqtt_server, json["mqtt_server"]);
            strcpy(mqtt_port, json["mqtt_port"]);
            strcpy(mqtt_user, json["mqtt_user"]);
            strcpy(mqtt_password, json["mqtt_password"]);
            strcpy(mqtt_topic, json["mqtt_topic"]);            
            strcpy(blynk_token, json["blynk_token"]);
            strcpy(captive_user, json["captive_user"]);            
            strcpy(captive_password, json["captive_password"]);
          } else {
            Serial.println("failed to load json config");
          }
        }
      } else {
        Serial.println("Config file does not exist.");
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
    WiFiManagerParameter custom_captive_user("captiveuser", "captive user", captive_user, 40);
    WiFiManagerParameter custom_captive_password("captivepassword", "captive password", captive_password, 40);

   
    WiFiManager wifiManager;
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_password);
    wifiManager.addParameter(&custom_mqtt_topic);
    wifiManager.addParameter(&custom_blynk_token);
    wifiManager.addParameter(&custom_captive_user);
    wifiManager.addParameter(&custom_captive_password);


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
    strcpy(captive_user, custom_captive_user.getValue());
    strcpy(captive_password, custom_captive_password.getValue());

    

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
    json["captive_user"] = captive_user;
    json["captive_password"] = captive_password;
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
}


String urlencode(String str) {
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}

void authenticate() {
  WiFiClientSecure client;
  
  while (!client.connect("7.7.7.7", 443)) {
    Serial.println("connection failed");
    delay(500);
  }
  
  String url = "/login.html?redirect=go.microsoft.com/fwlink/?LinkID=219472&clcid=0x409";
  String postParams = "buttonClicked=4&err_flag=0&err_msg=&info_flag=0&info_msg=&redirect_url=http%3A%2F%2Fgo.microsoft.com%2Ffwlink%2F%3FLinkID%3D219472%26clcid%3D0x409&network_name=Guest+Network&username=" +
  urlencode(captive_user) + 
  "&password=" +
  urlencode(captive_password);

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: 7.7.7.7\r\n" +
               "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n" +
               "Connection: close\r\n"
               "Content-Length: " + postParams.length() + "\r\n\r\n" +
               postParams);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    //Serial.println(line);
  }
}

bool checkConnection() {
      // Use WiFiClient class to create TCP connections
    WiFiClient clienthttp;

    if (!clienthttp.connect("gstatic.com", 80)) {
        Serial.println("connection to http://gstatic.com failed");
        Serial.println("wait 5 sec...");
        delay(5000);
        return false;
    }

      clienthttp.print(String("GET /generate_204 HTTP/1.1\r\n") +
               "Host: gstatic.com\r\n" +
               "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent to gstatic.com");
  while (clienthttp.connected()) {
    String line = clienthttp.readStringUntil('\n');
    //Serial.println(line);
    if (line.lastIndexOf("204 No Content") != -1) {
      Serial.println("Connected to Internet");
      return true;
    }
  }
  Serial.println("No internet connection is available");
  return false;

}


void timer0_ISR (void) {
  Serial.println("timer0_ISR called. Resetting ESP...");
  ESP.reset();
}

void setup() {
  Serial.begin(115200);

   //SPIFFS.format();
  
  pinMode(TRIGGER_PIN, INPUT);
  start = millis();


  if (digitalRead(TRIGGER_PIN) == HIGH) {
        Serial.println("Starting in configuration mode");
        isConfig = true;
  }

  reloadConfiguration();

  if (!isConfig) {
    noInterrupts();
    timer0_isr_init();
    timer0_attachInterrupt(timer0_ISR);
    timer0_write(ESP.getCycleCount() + 80000000L * 30); // 80MHz * 30 == 30sec
    interrupts();

    establishWiFi();
    if (!checkConnection()) {
      authenticate();
    }
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

