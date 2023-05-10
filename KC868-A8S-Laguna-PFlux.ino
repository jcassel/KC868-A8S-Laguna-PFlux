#define VERSIONINFO "1.0.7"
#define COMPATIBILITY "SIOPlugin 0.1.1"
#define DEFAULT_HOSTS_NAME "CMSShopController-New"

#include <PCF8574.h>
#include "TimeRelease.h"
#include <ArduinoJson.h> 
#include <SPIFFS.h>   // Filesystem support header
#include <Bounce2.h>
#include "FS.h"
#include <WiFi.h>
#include <NTPClient.h>
#include <Time.h>        
#include <TimeLib.h>
#include <WebServer.h>
#include <MQTT.h>
#include "global.h"
#include "wifiCommon.h"
#include "MQTTCommon.h"

#define FS_NO_GLOBALS
#include "webPages.h"



void setup() {
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);   //beep
  Serial.begin(115200);
  Serial2.begin(115200);
  InitStorageSetup();

  if(!loadSettings()){
    Serial.println("Settings Config Loaded");
  }
  
  SetupWifi();
  DebugwifiConfig();
  SetupIO();


  debugMsg("begin init pages");
  initialisePages();
  debugMsg("end init pages");
  webServer.begin();
  debugMsg("end webserver.begin");

  randomSeed(micros());
  
  //MQTT Setup
  if(settingsConfig.MQTTEN == 1){
    mqttReconnect.set(10000UL); //check in 10sec on startup.
    mqttClient.begin(settingsConfig.MQTTSrvIp, espClient);
    mqttClient.onMessage(mqttMessageReceived);
    mqttStatusInerval.set(settingsConfig.StatusIntervalSec * 1000);
    //mqttResendCommand.set(mqttResendCommandInterval);  
    mqttResendCommand.clear(); //not running yet.
  }else{
    Serial.println("MQTT Not Enabled");
  }
  
  //checkBlastGates();
  
  mqttResendCommand.set(mqttResendCommandInterval);
  Serial.println("Setup Complete");
}

int setOut = LOW;
int IOSize = 8;
//******************************strtloop******************************************
void loop() {
  checkSerial();
  CheckWifi();
  //int in = Inputs.digitalRead(0);
  //Outputs.digitalWrite(0, setOut);
  
  checkBlastGates();
  delay(300);//give lower priority tasks a chance.Really need this for the IO to work properly... Without a good bit of delay, it seems to get false positives for Input status. 
  yield();//give higher priority tasks a change.
  mqttCheckConnection();

  
}
//******************************endloop*******************************************


int dustCollectorCommandCount = 0;
void checkBlastGates(){
  if(_debug){Serial.println("Checking BlastGates");}
  if(mqttResendCommand.check()){
    if(checkInputs()){//Switch closed pulls the input to ground.
      if(_debug){
      Serial.println("At least one BlastGate is Open");
      Serial.println("Attempting to sending Start Command");
      }
      if(settingsConfig.MQTTEN == 1){
        if(dustCollectorOnState != true){
          dustCollectorCommandCount = 0;
        }
        if(dustCollectorCommandCount < dustCollectorCommandMaxCount){
          mqttSendCommand("On");
          dustCollectorCommandCount++;
        }
        dustCollectorOnState = true;
      }
    }else{
      if(_debug){
      Serial.println("No BlastGates are open");
      Serial.println("Attempting to sending Stop Command");
      }
      
     if(settingsConfig.MQTTEN == 1){
        if(dustCollectorOnState != false){
          dustCollectorCommandCount = 0;
        }
        if(dustCollectorCommandCount < dustCollectorCommandMaxCount){
          mqttSendCommand("Off");
          dustCollectorCommandCount++;
        }
        dustCollectorOnState = false;
      }
    }
    mqttResendCommand.set(mqttResendCommandInterval);
  }
  
}
  

void checkSerial(){
  if (Serial.available()){
    
    String buf = Serial.readStringUntil('\n');
    buf.trim();
    if(_debug){debugMsg("buf:" + buf);}
    int sepPos = buf.indexOf(" ");
    String command ="";
    String value = "";
    
    if(sepPos){
      command = buf.substring(0,sepPos);
      value = buf.substring(sepPos+1);;
      if(_debug){
        debugMsg("command:[" + command + "]");
        debugMsg("value:[" + value+ "]");
      }
    }else{
      command = buf;
      if(_debug){
        debugMsg("command:[" + command + "]");
      }
    }
    
    if(command == "Echo"){ 
      ack();
      Serial.println(value);
      return;
    }
     else if(command == "WF"){
      
      if(value == "netstat"){ack();PrintNetStat();}
      if(value.startsWith("set")){
        ack();
        WifiConfig wfconfig;
        
        //wfconfig.ssid = wifiConfig.ssid;
        strlcpy(wfconfig.ssid, wifiConfig.ssid, sizeof(wfconfig.ssid));
        //wfconfig.password = wifiConfig.password;
        strlcpy(wfconfig.password, wifiConfig.password, sizeof(wfconfig.password));
        //wfconfig.wifimode = wifiConfig.wifimode;
        strlcpy(wfconfig.wifimode, wifiConfig.wifimode, sizeof(wfconfig.wifimode));
        //wfconfig.hostname = wifiConfig.hostname;
        strlcpy(wfconfig.hostname, wifiConfig.hostname, sizeof(wfconfig.hostname));
        wfconfig.attempts = wifiConfig.attempts;
        wfconfig.attemptdelay = wifiConfig.attemptdelay;
        //wfconfig.apPassword = wifiConfig.apPassword;
        strlcpy(wfconfig.apPassword, wifiConfig.apPassword, sizeof(wfconfig.apPassword));
        String nameValuePare = value.substring(value.indexOf(" ")+1);
        String setting = nameValuePare.substring(0,nameValuePare.indexOf(" "));
        String newValue = nameValuePare.substring(nameValuePare.indexOf(" ")+1);
        if(_debug){
        Serial.print("Wifi Setting entered:");Serial.println(setting);
        Serial.print("Wifi value entered:");Serial.println(newValue);
        }
        if(setting == "ssid"){
          //wfconfig.ssid = newValue;
          strlcpy(wfconfig.ssid, newValue.c_str(), sizeof(wfconfig.ssid));
        }else if (setting == "password"){
          //wfconfig.password = newValue;
          strlcpy(wfconfig.password, newValue.c_str(), sizeof(wfconfig.password));
        }else if (setting == "hostname"){
          //wfconfig.hostname = newValue;
          strlcpy(wfconfig.hostname, newValue.c_str(), sizeof(wfconfig.hostname));
        }else if (setting == "attempts"){
          wfconfig.attempts = newValue.toInt();
        }else if (setting == "attemptdelay"){
          wfconfig.attemptdelay = newValue.toInt();
        }else if (setting == "apPassword"){
          //wfconfig.apPassword = newValue;
          strlcpy(wfconfig.apPassword, newValue.c_str(), sizeof(wfconfig.apPassword));
        }else if (setting == "wifimode"){
          // WIFI_AP or WIFI_STA
          if(newValue == "STA"){
            //wfconfig.wifimode = "WIFI_STA";
            strlcpy(wfconfig.wifimode, "WIFI_STA", sizeof(wfconfig.wifimode));
          }else if(newValue == "AP"){
            //wfconfig.wifimode = "WIFI_AP";
            strlcpy(wfconfig.wifimode, "WIFI_AP", sizeof(wfconfig.wifimode));
          }
        }
        if(_debug){
          debugMsg("saving updates to wifiConfig");
        }
        savewifiConfig(wfconfig);
        loadwifiConfig();
      }
    }    
  }
}

void ack(){
  Serial.println("OK");
}
