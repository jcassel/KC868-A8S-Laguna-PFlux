void mqttSendCommand(const String &command){
  String msgChan = "woodShop/dustCollection"; 
  mqttClient.publish(msgChan,command);
  Serial.println("Publish:" + msgChan + ":" + command );
}



void mqttTele(const String &subject, const String &message){
  if(mqttClient.connected()){
    mqttClient.publish("tele/"+ String(wifiConfig.hostname) +"/"+ subject,message);
    Serial.println("Sending MQTTInfo");
  }
}


void mqttLWT(bool setWill){
  
    String topic = "tele/"+ String(wifiConfig.hostname) +"/LWT";
    if(setWill){
      mqttClient.setWill(topic.c_str(), "Offline",true,2);
      Serial.println("Set Will: Offline");
    }else{
      if(mqttClient.connected()){
        mqttClient.publish(topic.c_str(), "Online",true,0);
    }
  }
}
  
boolean mqttConnectOptions(){
  boolean result;
  mqttLWT(true); //set last will and testament
  if (settingsConfig.MQTTPW != "")
  {
    Serial.print("Connecting to MQTT Server with UserName and Password: ");
    result = mqttClient.connect(wifiConfig.hostname, settingsConfig.MQTTUser, settingsConfig.MQTTPW);
    
  }
  else if (settingsConfig.MQTTUser != "")
  {
    Serial.print("Connecting to MQTT Server with UserName no password: ");
    result = mqttClient.connect(wifiConfig.hostname, settingsConfig.MQTTUser);
    
  }
  else
  {
    Serial.print("Connecting to MQTT Server with no credentials: ");
    result = mqttClient.connect(wifiConfig.hostname);
    
  }
  Serial.println(result);
  return result;
}

void mqttStatus(){
  if(mqttClient.connected()){
    mqttClient.publish("stat/"+ String(wifiConfig.hostname) +"/State","Ready");
    mqttTele("IP",IpAddress2String(WiFi.localIP()));
    Serial.println("Sending MQTTStatus");
  }
}

boolean mqttConnect() {
  Serial.println("Attempting Connection to MQTT server...");
  if (!mqttConnectOptions()) {
    Serial.println("Attempt Failed");
    return false;
  }
  Serial.println("Connected!");
    
  mqttLWT(false); //send current Testiment Of Online
  mqttStatus();
  
  //Subscribe to command channel
  mqttClient.subscribe("cmnd/"+ String(wifiConfig.hostname) + "/#");
  
  return true;
  
}

void mqttMessageReceived(String &topic, String &payload){
  Serial.println("Incoming: " + topic + " - " + payload);

  if(topic.endsWith("Info")){
    if(payload == "IP"){
       mqttTele("IP",IpAddress2String(WiFi.localIP()));
    }
  }
}


bool mqttCheckConnection(){
  if(isWiFiConnected()){ 
    if(!mqttClient.connected()){
      Serial.println("WiFi is connected and MQTTClient is not connected");
      if(mqttReconnect.check()){ //has a connection attempt been made reciently
        Serial.println("Attempting MQTT (re)connect");
        mqttConnect();  
        mqttReconnect.set(30000UL); //30 seconds
      }else{
        if(String(mqttReconnect.timeLeft()).endsWith("0")){
        Serial.print("MQTT Reconnect timer running: ");
        Serial.println(mqttReconnect.timeLeft());
        }
      }
    }
  }
  
  return mqttClient.connected();
}
