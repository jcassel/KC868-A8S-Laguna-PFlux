bool InAPMode = false;
bool APModeSuccess = false;
bool needReset = false;
bool doWiFiScan = false;
bool WifiInitialized = false;

TimeRelease ReconnectWiFi;
TimeRelease resetTimeDelay;


uint32_t getChipId(){
  uint32_t chipId = 0;
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  
  return chipId;
}

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and no offset
//int TimeZoneOffsetHours = -5; Now in the settingsconfig struct//Eastern(US NY) standard time [This value will default to -5 but can be over ridden in the settings.d file
NTPClient timeClient(ntpUDP,"pool.ntp.org",(-5 * 3600/-1),120000);// -4 hours update every 2 minutes gets updated in setup.

struct WifiConfig
{
  char ssid[64];
  char password[64];
  char wifimode[12];
  char hostname[64];
  int attempts;
  int attemptdelay;
  char apPassword[64];
};

struct SettingsConfig
{
  bool MQTTEN;
  char MQTTSrvIp[128];
  int MQTTPort;
  char MQTTUser[34];
  char MQTTPW[34];
  char MQTTTopic[64];
  int StatusIntervalSec;
  int TimeZoneOffsetHours;
};


SettingsConfig settingsConfig;
WifiConfig wifiConfig;
char* www_username = "admin";
WebServer webServer(80);
int lastWifiStatus = -1;


bool isWiFiConnected(){

  if(lastWifiStatus !=WiFi.status()){ // should only print out the status when it changes.
    lastWifiStatus =WiFi.status();
    #ifdef _GDebug
      Serial.print("WiFi Status: ");
      Serial.println(WiFi.status());
    #endif
  }
  return (WiFi.status() == WL_CONNECTED);
}


bool startWifiStation(){
  
  Serial.printf("[INFO]: Connecting to %s\n", wifiConfig.ssid);
  Serial.printf("Existing set WiFi.SSID:%s\n",WiFi.SSID().c_str());
  Serial.printf("wifiConfig.ssid:%s\n",wifiConfig.ssid);
  
  if(String(wifiConfig.wifimode) == "WIFI_STA"){
    if ( String(WiFi.SSID()) != String(wifiConfig.ssid) || WifiInitialized == false)
    {
        
        Serial.println("initiallizing WiFi Connection");
        
        if(isWiFiConnected()){
          WiFi.disconnect();
        }
        
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiConfig.ssid, wifiConfig.password);
        WifiInitialized = true;
        uint8_t attempts = wifiConfig.attempts;
        while (!isWiFiConnected())
        {
          if(attempts == 0) {
            WiFi.disconnect();
            Serial.println("");
            return false;
            
          }
          
          delay(wifiConfig.attemptdelay);
          Serial.print(".");
          attempts--;
       }
       Serial.println(WiFi.localIP());  
       
    }else{
      if(WiFi.status() != WL_CONNECTED){
        WiFi.reconnect();
      }
    }
    WiFi.hostname(wifiConfig.hostname);
  }else{
    
    return false; //In AP mode.. should not be connecting.
  }
  
  Serial.print("WiFi.Status(): ");
  Serial.println(String(WiFi.status()));
  Serial.print("WL_CONNECTED: ");
  Serial.println(String(WL_CONNECTED));
  
  //needMqttConnect = (WiFi.status() == WL_CONNECTED); //get the MQtt System to connect once wifi is connected.
  return (WiFi.status() == WL_CONNECTED);
}



void DoInAPMode(){
  if(InAPMode && !APModeSuccess){
    //Going into AP mode
    
    Serial.println("Entering AP mode.");
    
    if(wifiConfig.hostname == DEFAULT_HOSTS_NAME){
      String tempName = wifiConfig.hostname + String(getChipId()); 
      strlcpy(wifiConfig.hostname,tempName.c_str(),sizeof(wifiConfig.hostname));
    }
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    delay(100);
    APModeSuccess = WiFi.softAP(wifiConfig.hostname, wifiConfig.apPassword);
    if (APModeSuccess){
    
      Serial.print("SoftAP IP Address: ");
      Serial.println(WiFi.softAPIP());
    
      ReconnectWiFi.set(15000); //15 seconds
    }else{
      APModeSuccess = true; // not really but if we do not do this, It will come though here and reset the reset delay every loop. 
    
      Serial.println("SoftAP mode Failed Rebooting in 15 seconds.");
    
      resetTimeDelay.set(15000UL); //trigger 15 sec
      needReset = true;
    }
  }
}

void CheckWifi(){
   //check connection status
  if(!InAPMode || InAPMode && !APModeSuccess){
    if(!isWiFiConnected()){
      InAPMode = true; 
      DoInAPMode();
    }else{ //service requests
      webServer.handleClient();
    }
    
  }else{
    //only try to reconnect if no one has connected as an AP client.
    if(ReconnectWiFi.check() && WiFi.softAPgetStationNum() ==0){
        InAPMode = !startWifiStation(); //attempt to reconnect to the main wifi access point if it succeds, Set to false.
        APModeSuccess = false; //reset so it will do a full attempt to go into AP mode if called to
    }
  }
  
  
  if(resetTimeDelay.check()){
    if(needReset){
      delay(10);
      ESP.restart();
    }
  }
}


String IpAddress2String(const IPAddress& ipAddress)
{
    return String(ipAddress[0]) + String(".") +
           String(ipAddress[1]) + String(".") +
           String(ipAddress[2]) + String(".") +
           String(ipAddress[3]);
}

void DebugwifiConfig(){
  Serial.print("wifiConfig.ssid: ");
  Serial.println(wifiConfig.ssid);
  Serial.print("wifiConfig.password: ");
  Serial.println(wifiConfig.password);
  Serial.print("wifiConfig.wifimode: ");
  Serial.println(wifiConfig.wifimode);
  Serial.print("wifiConfig.hostname: ");
  Serial.println(wifiConfig.hostname);
  Serial.print("wifiConfig.attempts: ");
  Serial.println(wifiConfig.attempts);
  Serial.print("wifiConfig.attemptdelay: ");
  Serial.println(wifiConfig.attemptdelay);
  Serial.print("wifiConfig.apPassword: ");
  Serial.println(wifiConfig.apPassword);
}

String wifiConfigFile = "/config/wifiConfig.json";
bool loadwifiConfig()
{
  
    Serial.print("wifiConfig file path: ");Serial.println(wifiConfigFile);
  
  if (!SPIFFS.exists(wifiConfigFile))
  {
   
    Serial.println("[WARNING]: wifiConfig file not found! Loading Factory Defaults.");
  
    strlcpy(wifiConfig.ssid,"unset", sizeof(wifiConfig.ssid));
    strlcpy(wifiConfig.password, "", sizeof(wifiConfig.password));
    
    strlcpy(wifiConfig.wifimode, "WIFI_AP", sizeof(wifiConfig.wifimode));
    strlcpy(wifiConfig.hostname, DEFAULT_HOSTS_NAME, sizeof(wifiConfig.hostname));
    strlcpy(wifiConfig.apPassword, "Password1", sizeof(wifiConfig.apPassword));
    wifiConfig.attempts = 10;
    wifiConfig.attemptdelay = 5000; //5 second delay
    return false;
  }
  
  File configfile = SPIFFS.open(wifiConfigFile,"r");

  DynamicJsonDocument doc(1024);

  DeserializationError error = deserializeJson(doc, configfile);

  strlcpy(wifiConfig.ssid, doc["ssid"] | "FAILED", sizeof(wifiConfig.ssid));
  strlcpy(wifiConfig.password, doc["password"] | "FAILED", sizeof(wifiConfig.password));
  strlcpy(wifiConfig.wifimode, "WIFI_STA", sizeof(wifiConfig.wifimode));
  strlcpy(wifiConfig.hostname, doc["hostname"] | DEFAULT_HOSTS_NAME, sizeof(wifiConfig.hostname));
  strlcpy(wifiConfig.apPassword, doc["apPassword"] | "Password1", sizeof(wifiConfig.apPassword));

  int attempts = doc["attempts"] | 10 ;
  wifiConfig.attempts = attempts;

  int attemptdelay = doc["attemptdelay"] | 5000 ;
  wifiConfig.attemptdelay = attemptdelay;

  configfile.close();

  if (error)
  {
    Serial.println("[ERROR]: deserializeJson() error in loadwifiConfig");
    Serial.println(error.c_str());
    return false;
  }else{
  
    Serial.println("wifiConfig was loaded successfully");
    DebugwifiConfig();
    
  }
  

  return true;
}


bool savewifiConfig(WifiConfig newConfig)
{
  
  Serial.print("wifiConfig file path: ");Serial.println(wifiConfigFile);
  
  SPIFFS.remove(wifiConfigFile);
  File file = SPIFFS.open(wifiConfigFile, "w");

  DynamicJsonDocument doc(1024);

  JsonObject wifiConfigobject = doc.to<JsonObject>();
  
  wifiConfigobject["ssid"] = newConfig.ssid;
  wifiConfigobject["password"] = newConfig.password;
  wifiConfigobject["wifimode"] = newConfig.wifimode;
  wifiConfigobject["hostname"] = newConfig.hostname;
  wifiConfigobject["attempts"] = newConfig.attempts;
  wifiConfigobject["attemptdelay"] = newConfig.attemptdelay;
  wifiConfigobject["apPassword"] = newConfig.apPassword;
  int chrW = serializeJsonPretty(doc, file);
  if (chrW == 0)
  {
    Serial.println("[WARNING]: Failed to write to webconfig file");
    return false;
  }else {
    wifiConfig = newConfig;
  
    Serial.print("Characters witten: ");Serial.println(chrW);
    Serial.println("wifiConfig was updated");
    DebugwifiConfig();
  
  }
    
  file.close();
  return true;
}


void SetupWifi(){
  if(loadwifiConfig()){
  
    Serial.println("WiFi Config Loaded");
  
    InAPMode  = false;
  }else{
  
    Serial.println("WiFi Config Failed to Load");
  
    InAPMode  = true;
  }
  if(InAPMode){
    DoInAPMode();
  }else{
    startWifiStation();
  }

  Serial.println("Wifi Setup complete");

}

void PrintNetStat(){
  Serial.print("Is Wifi Connected: ");
  Serial.println();
  if (isWiFiConnected()){
    Serial.println("yes");
    Serial.print("IP:");
    Serial.println(WiFi.localIP());
    
    Serial.print("SubNet:");
    Serial.println(WiFi.subnetMask());

    Serial.print("Gateway:");
    Serial.println(WiFi.gatewayIP());
    //would like to add other info here. Things like gateway and dns server and Maybe MQTT info
  }else{
    Serial.println("No");
  }
}

void DebugSettingsConfig(){
  Serial.print("settingsConfig.MQTTEN: ");
  Serial.println(settingsConfig.MQTTEN);
  
  Serial.print("settingsConfig.MQTTSrvIp: ");
  Serial.println(settingsConfig.MQTTSrvIp);
  
  Serial.print("settingsConfig.MQTTPort: ");
  Serial.println(settingsConfig.MQTTPort);

  Serial.print("settingsConfig.MQTTUser: ");
  Serial.println(settingsConfig.MQTTUser);

  Serial.print("settingsConfig.MQTTPW: ");
  Serial.println(settingsConfig.MQTTPW);
  
  Serial.print("settingsConfig.MQTTTopic: ");
  Serial.println(settingsConfig.MQTTTopic);
  

  Serial.print("settingsConfig.StatusIntervalSec: ");
  Serial.println(settingsConfig.StatusIntervalSec);

  Serial.print("settingsConfig.TimeZoneOffsetHours: ");
  Serial.println(settingsConfig.TimeZoneOffsetHours);
}

String settingsConfigFile = "/config/settingsConfig.json";
bool loadSettings(){

  Serial.print("SettingsConfig file path: ");Serial.println(settingsConfigFile);

  
  if (!SPIFFS.exists(settingsConfigFile))
  {

    Serial.println("[WARNING]: SettingsConfig file not found! Loading Factory Defaults.");

    settingsConfig.MQTTEN = false;
    //strlcpy(settingsConfig.MQTTEN,"0", sizeof(settingsConfig.MQTTEN));
    strlcpy(settingsConfig.MQTTSrvIp,"", sizeof(settingsConfig.MQTTSrvIp));
    
    settingsConfig.MQTTPort = 1883 ; 
    //strlcpy(settingsConfig.MQTTPort,"1883", sizeof(settingsConfig.MQTTPort));
    
    strlcpy(settingsConfig.MQTTUser, "", sizeof(settingsConfig.MQTTUser));
    strlcpy(settingsConfig.MQTTPW, "", sizeof(settingsConfig.MQTTPW));

    
    strlcpy(settingsConfig.MQTTTopic, "", sizeof(settingsConfig.MQTTTopic));

    settingsConfig.StatusIntervalSec = 5;
    settingsConfig.TimeZoneOffsetHours = -5; //eastern(NewYork) time 
    return false;
  }
  
  File configfile = SPIFFS.open(settingsConfigFile,"r");
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, configfile);
  
  bool MQTTEN = doc["MQTTEN"];
  settingsConfig.MQTTEN = MQTTEN;
  
  //strlcpy(settingsConfig.MQTTEN,doc["MQTTEN"], sizeof(settingsConfig.MQTTEN));
  strlcpy(settingsConfig.MQTTSrvIp, doc["MQTTSrvIp"] | "", sizeof(settingsConfig.MQTTSrvIp));
 
  //uint8_t MQTTPort = doc["MQTTPort"];
  //settingsConfig.MQTTPort = MQTTPort;
  settingsConfig.MQTTPort = doc["MQTTPort"];
  //strlcpy(settingsConfig.MQTTPort,doc["MQTTPort"], sizeof(settingsConfig.MQTTPort));
  
  strlcpy(settingsConfig.MQTTUser, doc["MQTTUser"] | "", sizeof(settingsConfig.MQTTUser));
  strlcpy(settingsConfig.MQTTPW, doc["MQTTPW"] | "", sizeof(settingsConfig.MQTTPW));
  
  strlcpy(settingsConfig.MQTTTopic, doc["MQTTTopic"] | "", sizeof(settingsConfig.MQTTTopic));
  

  int StatusIntervalSec = doc["StatusIntervalSec"] | 15 ;
  settingsConfig.StatusIntervalSec = StatusIntervalSec;
  
  int TimeZoneOffsetHours = doc["TimeZoneOffsetHours"] | -5 ;
  settingsConfig.TimeZoneOffsetHours = TimeZoneOffsetHours;

  configfile.close();

  if (error)
  {
    Serial.println("[ERROR]: deserializeJson() error in loadSettings");
    Serial.println(error.c_str());
    return false;
  }else{

    Serial.println("SettingsConfig was loaded successfully");
    DebugSettingsConfig();

  }
  

  return true;  
}

bool saveSettings(SettingsConfig newSettings){
  Serial.print("settingsConfig file path: ");Serial.println(settingsConfigFile);

  
  SPIFFS.remove(settingsConfigFile);
  File file = SPIFFS.open(settingsConfigFile, "w");

  DynamicJsonDocument doc(1024);

  JsonObject Configobject = doc.to<JsonObject>();
  
  Configobject["MQTTEN"] = newSettings.MQTTEN;
  Configobject["MQTTSrvIp"] = newSettings.MQTTSrvIp;
  Configobject["MQTTPort"] = newSettings.MQTTPort;
  Configobject["MQTTUser"] = newSettings.MQTTUser;
  Configobject["MQTTPW"] = newSettings.MQTTPW;
  Configobject["MQTTTopic"] = newSettings.MQTTTopic;
  
  
  Configobject["StatusIntervalSec"] = newSettings.StatusIntervalSec;
  Configobject["TimeZoneOffsetHours"] = newSettings.TimeZoneOffsetHours;
  
  int chrW = serializeJsonPretty(doc, file);
  if (chrW == 0)
  {
    Serial.println("[WARNING]: Failed to write to settingsConfi file");
    return false;
  }else {
    settingsConfig = newSettings;
    #ifdef _GDebug
    Serial.print("Characters witten: ");Serial.println(chrW);
    Serial.println("settingsConfig was updated");
    DebugSettingsConfig();
    #endif
  }
    
  file.close();
  return true;
}
