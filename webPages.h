#include <SPIFFS.h>   // Filesystem support header

String LastStatus = "Ready!"; //latest status message.
String Networks = "";

//takes the value from the wifi scan and turns it into a percentage signal strength
int dBmtoPercentage(int dBm)
{
  int quality;
    if(dBm <= -100)
    {
        quality = 0;
    }
    else if(dBm >= -50)
    {  
        quality = 100;
    }
    else
    {
        quality = 2 * (dBm + 100);
   }

     return quality;
}//dBmtoPercentage 


void storeWifiScanResult(int networksFound) {
  
  Networks = "{\"networks\":[";
  for (int i = 0; i < networksFound; i++){

    if((i + 1)< networksFound){
      Networks += "\"";
      Networks += WiFi.SSID(i);
      Networks += " (";
      Networks += dBmtoPercentage(WiFi.RSSI(i));
      Networks += "%)\",";
    }else{
      Networks += "\"";
      Networks += WiFi.SSID(i);
      Networks += " (";
      Networks += dBmtoPercentage(WiFi.RSSI(i));
      Networks += "%)\"";
    }
    
    Serial.printf("%d: %s, Ch:%d (%ddBm) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "");
    
    
  }
  
  Networks += "]}";
  
  //WiFi.scanDelete(); not supported for esp8266
  
}

const char* www_realm = "Login Required";
String authFailResponse = "Authentication Failed";
bool checkAuth(){
  
  if(!webServer.authenticate(www_username,wifiConfig.apPassword)){
    #ifdef USE_DIGESTAUTH
    webServer.requestAuthentication(DIGEST_AUTH, www_realm, authFailResponse);
    Serial.print("AuthFailed?:");
    Serial.println(authFailResponse);
    #else
    webServer.requestAuthentication();
    #endif

    return webServer.authenticate("admin",wifiConfig.apPassword);
    
  }else{
    return true;  
  }
  
}
bool handleFileRead(String path) {
  if(!checkAuth())
    return false;
  
  Serial.println("start handleFileRead");
  
  
  if (SPIFFS.exists(path)) {
    
    Serial.print("File found:");
    Serial.println(path);
    
    File file = SPIFFS.open(path, "r");
    if(path.endsWith("css")){
      webServer.streamFile(file, "text/css");
    }else{
      webServer.streamFile(file, "text/html");
    }
    file.close();
    return true;
  }else{
    
    Serial.print("File not found");
    Serial.println(path);
    
  }
  return false;
}

File fsUploadFile;  //SPIFFS file object to hold the new file info.
void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = webServer.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    LastStatus = filename; //put this here for use later when done the upload.
    if(!filename.startsWith("/")) filename = "/"+filename;
    #ifdef _debugopra
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    #endif
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      #ifdef _debugopra
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      #endif
      
      LastStatus = "File Upload success:" + LastStatus + ":" + String(upload.totalSize) + " bytes"; 
      webServer.sendHeader("Location","/spiffs");      // Redirect so that it refreshes
      webServer.send(303);
    } else {
      webServer.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void GetFileList(String &PageContent){
  File dir = SPIFFS.open ("/");
  File file = dir.openNextFile();
  PageContent += "<p>";
  while (file) {
      PageContent += "<a href='/spiffsDL?download=";
      PageContent += file.name();
      PageContent += "'>";
      PageContent += file.name();
      PageContent += " ";
      PageContent += file.size();
      PageContent += "</a>";
      PageContent += "<br/>";
      file = dir.openNextFile();
  }
  
  PageContent += "<p>";
  return;
}


 
 void getSpiffsPage(String &page){
    page = "<!DOCTYPE HTML><html><head><title>SPIFFS UPload</title><body>";
    page += "<form method='post' enctype='multipart/form-data'><input type='file' name='name'><input class='button' type='submit' value='Upload'></form>";
    GetFileList(page);
    page += "<p>Last Status: " + LastStatus + "</p>";
    page += "<p><a href='/config'>Return to Config</a></p>";
    page += "</body></html>";
}

bool readFileListToJsonArray(String& outPut){
    
    File dir = SPIFFS.open("/");
    File file = dir.openNextFile();
    while (file) {
        outPut += "{\"path\":\"";
        outPut += file.name();
        outPut += "\",\"size\":\"";
        outPut += file.size();
        outPut += "\"},";
        file = dir.openNextFile();
    }
    Serial.print("FileListOutput: ");
    Serial.println(outPut);
    outPut = outPut.substring(0,outPut.length()-1); //remove the last comma
    return true;
}



void initialisePages(){
  
  Serial.println("Initializing pages start");
  
  webServer.serveStatic("/config.js",SPIFFS,"/config.js");//.setAuthentication(www_username,wifiConfig.apPassword);
  webServer.serveStatic("/ioconfig.js",SPIFFS,"/ioconfig.js");//.setAuthentication(www_username,wifiConfig.apPassword);
  webServer.serveStatic("/style.css",SPIFFS,"/style.css");//.setAuthentication(www_username,wifiConfig.apPassword);//31536000
  
  webServer.on("/APIGetFileList",HTTP_GET,[]{
    if(!checkAuth()){return ;}
    Serial.println("Start GetFileList");
    String files = "[";
    readFileListToJsonArray(files);
    files += "]";
    webServer.send(200, "application/json", files);
  });
  
  webServer.on("/config",HTTP_GET,[]{
    Serial.println("start config");
    
    handleFileRead("/config.htm");
    
    Serial.println("end config");
  });

   webServer.on("/APIDelFile",HTTP_GET,[]{
    if(!checkAuth()){return ;}

    LastStatus = "APIDelFile Called";
    if(webServer.hasArg("path")){
      String filePath = webServer.arg("path").c_str();
      #ifdef _debugopra
        Serial.println(filePath);
      #endif
      //protect some files
      //filePath == "/Control1.bmp" ||filePath == "/Home.bmp" ||filePath == "/Level.bmp" || filePath == "/Temps.bmp" ||
      if(filePath == "/mansettings.htm" ||filePath == "/mansettings.js" ||filePath == "/ota.htm" ||filePath == "/scripts.js" ||
        filePath == "/config/wificonfig.json" ||filePath == "/config/settings.json" ||filePath == "/style.css" ||filePath == "/spiffs.htm"){
        LastStatus = "Cannot deleted file: " + filePath;}
      else if(SPIFFS.exists(filePath)){
        if(SPIFFS.remove(filePath)){ //delete the file. 
          LastStatus = "File Deleted: " + filePath;
        }else{
          LastStatus = "Failed to Deleted file: " + filePath;
        }
      }
    }else{
      LastStatus = "Failed to Deleted file, no path provided";
    }
    
    webServer.send(200, "application/json", "{\"LastStatus\":\""+LastStatus+"\"}");
  });

    webServer.on("/spiffs",HTTP_GET,[]{
    if(!checkAuth()){return;}
    String page = "";
    if(!SPIFFS.exists("/spiffs.htm")){
      getSpiffsPage(page);
      webServer.send(200, "text/html",page );
    }else{
     handleFileRead("/spiffs.htm"); 
    }
  });

  webServer.on("/spiffs", HTTP_POST,[](){ 
    if(!checkAuth()){return;}
    
    webServer.send(200);},// Send status 200 (OK) to tell the client we are ready to receive
    handleFileUpload      // Receive and save the file
  );

  webServer.on("/spiffsDL",HTTP_GET,[]{
    if(!checkAuth()){return;}
    
    if(webServer.hasArg("download")){
       String fileName = webServer.arg("download").c_str();
       handleFileRead(fileName);
    }
  });


  
  webServer.on("/APIGetSettings",HTTP_GET,[]{
    if(!checkAuth()){return ;}
    Serial.println("StartAPIGetSettings");
    DebugSettingsConfig(); //prints the settings struct to the Serial line
      
    String settings = "{\"settings\":{";
    settings += "\"wifimode\":\"" + String(wifiConfig.wifimode) + "\"";
    settings += ",\"hostname\":\"" + String(wifiConfig.hostname) + "\"";
    settings += ",\"ssid\":\"" + String(wifiConfig.ssid) + "\"";
    settings += ",\"password\":\"" + String(wifiConfig.password) + "\"";
    settings += ",\"attempts\":\"" + String(wifiConfig.attempts) + "\"";
    settings += ",\"attemptdelay\":\"" + String(wifiConfig.attemptdelay) + "\"";
    settings += ",\"apPassword\":\"" + String(wifiConfig.apPassword) + "\"";
    
    
    settings += ",\"MQTTEN\":\"" + String(settingsConfig.MQTTEN) + "\"";
    settings += ",\"MQTTSrvIp\":\"" + String(settingsConfig.MQTTSrvIp) + "\"";
    settings += ",\"MQTTPort\":\"" + String(settingsConfig.MQTTPort) + "\"";
    settings += ",\"MQTTUser\":\"" + String(settingsConfig.MQTTUser) + "\"";
    settings += ",\"MQTTPW\":\"" + String(settingsConfig.MQTTPW) + "\"";
    settings += ",\"MQTTTopic\":\"" + String(settingsConfig.MQTTTopic) + "\"";
    
    
   
    settings += ",\"StatusIntervalSec\":\"" + String(settingsConfig.StatusIntervalSec) + "\"";
    settings += ",\"TimeZoneOffsetHours\":\"" + String(settingsConfig.TimeZoneOffsetHours) + "\"";
    settings += ",\"LastStatus\":\"" + LastStatus + "\"";
    
    settings += ",\"firmwareV\":\"" + String(VERSIONINFO)+ "\"";
    settings += "}}";
    
    webServer.send(200, "application/json", settings);
  });
  
  webServer.on("/getDeviceName",HTTP_GET,[]{
  if(!checkAuth()){ return;}
    
    String msg = String(wifiConfig.hostname) + ": " + String(VERSIONINFO);
    
    webServer.send(200, "text/plain", msg);
  });

  webServer.on("/APIScanWifi",HTTP_GET,[]{
    if(!checkAuth()){return ;}
      
    int n = WiFi.scanNetworks();
    storeWifiScanResult(n);
    doWiFiScan = true;
  
    Serial.print("Network Scanned requested:");
  
    webServer.send(200, "text/plain","OK" );
  });

  webServer.on("/APIGetNetworks",HTTP_GET,[]{
  if(!checkAuth()){return;}
    if(Networks.length() == 0){
      Serial.println("Sending Empty Network list");
      webServer.send(200, "application/json", "{\"networks\":[]}");
    }else{
      Serial.println("Sending Network list");
      
      webServer.send(200, "application/json", Networks);
      Networks = "";  
    }
  });

  webServer.on("/config",HTTP_POST,[]{
    if(!checkAuth()){return;}

    if(webServer.hasArg("update")){
      Serial.println("updating settings");
  
      strlcpy(wifiConfig.wifimode, webServer.arg("cbo_WFMD").c_str(), sizeof(wifiConfig.wifimode));
      strlcpy(wifiConfig.hostname, webServer.arg("tx_HNAM").c_str(), sizeof(wifiConfig.hostname));
      strlcpy(wifiConfig.ssid, webServer.arg("tx_SSID").c_str(), sizeof(wifiConfig.ssid));
      strlcpy(wifiConfig.password, webServer.arg("tx_WFPW").c_str(), sizeof(wifiConfig.password));
      wifiConfig.attempts = webServer.arg("tx_WFCA").toInt();
      wifiConfig.attemptdelay = webServer.arg("tx_WFAD").toInt();
      strlcpy(wifiConfig.apPassword, webServer.arg("tx_APW").c_str(), sizeof(wifiConfig.apPassword));
      
      
      Serial.println("AfterUpdate: wificonfig");
      DebugwifiConfig();
      

      //MQTT settings (future)
      
      settingsConfig.MQTTEN = webServer.arg("ch_MQTTEN").toInt();
      
      //strlcpy(settingsConfig.MQTTEN, webServer.arg("ch_MQTTEN").c_str(), sizeof(settingsConfig.MQTTEN));
            
      strlcpy(settingsConfig.MQTTSrvIp, webServer.arg("tx_MQTTSRV").c_str(), sizeof(settingsConfig.MQTTSrvIp));
      strlcpy(settingsConfig.MQTTUser, webServer.arg("tx_MQTTUSR").c_str(), sizeof(settingsConfig.MQTTUser));
      strlcpy(settingsConfig.MQTTPW, webServer.arg("tx_MQTTPW").c_str(), sizeof(settingsConfig.MQTTPW));

      strlcpy(settingsConfig.MQTTTopic, webServer.arg("tx_MQTTTopic").c_str(), sizeof(settingsConfig.MQTTTopic));
      
      
      settingsConfig.MQTTPort = webServer.arg("tx_MQTTPort").toInt();
      //strlcpy(settingsConfig.MQTTPort, webServer.arg("tx_MQTTPort").c_str(), sizeof(settingsConfig.MQTTPort));

      settingsConfig.StatusIntervalSec = webServer.arg("tx_STI").toInt();
      settingsConfig.TimeZoneOffsetHours = webServer.arg("tx_TZOS").toInt();
      Serial.println("AfterUpdate: SettingsConfig");
      DebugSettingsConfig(); //prints the settings struct to the Serial line
      
      savewifiConfig(wifiConfig);
      saveSettings(settingsConfig);
      
      
      LastStatus = "Update Completed(Ready!)";
      
    }else if(webServer.hasArg("reboot")){
      Serial.println("web called a reboot");
      
      LastStatus = "Rebooting in 5 Sec";
      resetTimeDelay.set(10000UL); //need to give it more time so that the web page shows the message.
      needReset = true;
      
    }
    
    handleFileRead("/config.htm");

    Serial.println("end post config");

    
  });

  
}
