#define INPUTCOUNT 8
#define OUTPUTCOUNT 8
PCF8574 Outputs(0x24,4,5);
PCF8574 Inputs(0x22,4,5);

WiFiClient espClient;
MQTTClient mqttClient;

TimeRelease mqttReconnect;
TimeRelease mqttStatusInerval;
TimeRelease mqttResendCommand;
long mqttResendCommandInterval = 2000; //2 seconds
int dustCollectorCommandMaxCount = 5;
bool dustCollectorOnState = false;


bool _debug = false;

void SetupIO(){

  for (int i=0;i<INPUTCOUNT;i++){
    Inputs.pinMode(i, INPUT);  
  }
  for (int i=0;i<INPUTCOUNT;i++){
    Outputs.pinMode(i, OUTPUT);
  }
    
  Serial.print("Init pcf8574 i2C IO...");
  if (Inputs.begin()){
      Serial.println("PCF8674_input_OK");
  }else{
      Serial.println("PCF8674_input_KO");
  }
  if (Outputs.begin()){
      Serial.println("PCF8674_output_OK");
  }else{
      Serial.println("PCF8674_output_KO");
  }

}

int checkInput(int IOPoint){
  return Inputs.digitalRead(IOPoint);
}


bool checkInputs(){
  for (int i=0;i<INPUTCOUNT;i++){
    int inPutState = checkInput(i);
    if(_debug){
      Serial.print("Input: [");Serial.print(i);Serial.print("]-[");Serial.print(inPutState);Serial.println("]");
    }
    if( inPutState == 0){
      return true;
    }
  }
  
  return false;
}




void debugMsgPrefx(){
  Serial.print("DG:");
}

void debugMsg(String msg){
  debugMsgPrefx();
  Serial.println(msg);
}

float SpaceLeft()
{
  float freeMemory = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  return freeMemory;
}


bool IsSpaceLeft()
{
  float minmem = 100000.00; // Always leave 100 kB free pace on SPIFFS
  float freeMemory = SpaceLeft();
  String s = "[INFO]: Free memory left: "; s += freeMemory; s += "bytes"; debugMsg(s);
  
  if (freeMemory < minmem)
  {
    return false;
  }

  return true;
}

#define FORMAT_SPIFFS_IF_FAILED true
void InitStorageSetup(){
   if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      debugMsg("Warning SPIFFS Mount Failed (Attempt Restart if this is the first time this firmware was loaded.)");
      return;
   }
}
