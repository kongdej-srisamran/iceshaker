#include "modified_font.h"
#include "SSD1306.h" 
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"
#include "SPIFFS.h"

// Define Variables ------------------------------
#define LRELAY 13
#define RRELAY 12
#define MRELAY 14
#define MODE 26
#define DS18PIN 27
OneWire oneWire(DS18PIN);
DallasTemperature sensors(&oneWire);

#define ON 0
#define OFF 1
int auto_manual = 1; // 0 = auto, 1 = manual 
int timeout=0;
char rotate_time[] = "500"; // rotation time in ms
char stop_time[]   = "1000";// stop time in ms
char run_time[]    = "20";  // total run tim in minute
int r_time,s_time,u_time;
int r = 0;
int l = 0;
int relaystatus = 0;

unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 1000;  

unsigned int cmd=0;
float temperature;
#define FORMAT_SPIFFS_IF_FAILED true


// OLED -------------------------------------------
#define SDA_PIN 4   // GPIO21 -> SDA
#define SCL_PIN 15  // GPIO22 -> SCL
#define OLEDRST 16  // Reset
#define SSD_ADDRESS 0x3c
SSD1306  display(SSD_ADDRESS, SDA_PIN, SCL_PIN);

// SPIFFS ---------------------------------------------
int readFile(fs::FS &fs, const char * path){
    String line;
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return 0;
    }

    Serial.println("- read from file:");
    //while(file.available()){
   //     Serial.write(file.read());
        //return file.read();
    //}
    while(file.available()) {
      //Lets read line by line from the file
      line = file.readStringUntil('\n');
    }
    Serial.println(line);
    return line.toInt();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.println(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- frite failed");
    }
}
void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

int getSPIFF(char* file, char* val) {
  int d = readFile(SPIFFS, file); 
  if ( d == 0) {
    Serial.print("data init = ");Serial.println(val);
    writeFile(SPIFFS, file, val);
    return 0;
  }
  else {
    return d;
  }
}

// Line Thing -------------------------------------
#define DEVICE_NAME "Ice Shaker"
#define USER_SERVICE_UUID "5f961675-3050-4cef-8a98-2e7f58fb4e43"
#define PSDI_SERVICE_UUID "e625601e-9e55-4597-a598-76018a0d293d"
#define PSDI_CHARACTERISTIC_UUID "26e2b12b-85f0-4f3f-9fdd-91d114270e6e"
#define WRITE_CHARACTERISTIC_UUID "E9062E71-9E62-4BC6-B0D3-35CDCD9B027B"
#define NOTIFY_CHARACTERISTIC_UUID "62FBD229-6EDD-4D1A-B554-5C4E1BB29169"
BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;
BLECharacteristic* writeCharacteristic;
BLECharacteristic* notifyCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

class serverCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
   deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class writeCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *bleWriteCharacteristic) {
    std::string value = bleWriteCharacteristic->getValue();

    String str;
    
    for (uint8_t i = 0; i < value.length(); i++) {
        str += (char)value[i];
    }   
    Serial.println(str);
    
    String us = getValue(str, ',', 0);
    u_time = us.toInt();
    String ss = getValue(str, ',', 1);
    s_time = ss.toInt();
    String rs = getValue(str, ',', 2);
    r_time = rs.toInt();
    String c = getValue(str, ',', 3);
    cmd = c.toInt();
    
    char buf[100];
    rs.toCharArray(buf,rs.length() + 1);
    writeFile(SPIFFS, "/rotate.txt", buf);
    ss.toCharArray(buf,ss.length() + 1);
    writeFile(SPIFFS, "/stop.txt", buf);
    us.toCharArray(buf,us.length() + 1);
    writeFile(SPIFFS, "/run.txt", buf);
  }
  
  String getValue(String data, char separator, int index) {
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
  }
};

void setupServices(void) {
  // Create BLE Server
  thingsServer = BLEDevice::createServer();
  thingsServer->setCallbacks(new serverCallbacks());

  // Setup User Service
  userService = thingsServer->createService(USER_SERVICE_UUID);
  // Create Characteristics for User Service
  writeCharacteristic = userService->createCharacteristic(WRITE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
  writeCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  writeCharacteristic->setCallbacks(new writeCallback());

  notifyCharacteristic = userService->createCharacteristic(NOTIFY_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  notifyCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  BLE2902* ble9202 = new BLE2902();
  ble9202->setNotifications(true);
  ble9202->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  notifyCharacteristic->addDescriptor(ble9202);

  // Setup PSDI Service
  psdiService = thingsServer->createService(PSDI_SERVICE_UUID);
  psdiCharacteristic = psdiService->createCharacteristic(PSDI_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  psdiCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // Set PSDI (Product Specific Device ID) value
  uint64_t macAddress = ESP.getEfuseMac();
  psdiCharacteristic->setValue((uint8_t*) &macAddress, sizeof(macAddress));

  // Start BLE Services
  userService->start();
  psdiService->start();
}

void startAdvertising(void) {
  // Start Advertising
  BLEAdvertisementData scanResponseData = BLEAdvertisementData();
  scanResponseData.setFlags(0x06); // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  scanResponseData.setName(DEVICE_NAME);

  thingsServer->getAdvertising()->addServiceUUID(userService->getUUID());
  thingsServer->getAdvertising()->setScanResponseData(scanResponseData);
  thingsServer->getAdvertising()->start();
}
// END Line Thing ----------------------------------------


void onRelay(int n) {
//  Serial.println(n);
  if (n == RRELAY) {
    if (digitalRead(LRELAY) == LOW) {
      digitalWrite(RRELAY,HIGH); 
      Serial.println(" = R ON");
    }
  }
  else if (n == LRELAY) {
    if (digitalRead(RRELAY) == LOW) {
      digitalWrite(LRELAY,HIGH);   
      Serial.println(" = L ON");
    }    
  }
}

void runMotor() {

   unsigned long currentMillis = millis();
   int ds = r_time*2 + s_time*2;
   
   if (r == 0 && currentMillis - previousMillis >= ds) {
      Serial.println(currentMillis);
      onRelay(LRELAY);
      r = 1;
   }

   if (r == 1 && l == 0 && currentMillis - previousMillis >= (ds + r_time)) {
      Serial.println(currentMillis);
      digitalWrite(LRELAY,LOW);
      Serial.println(" = L OFF");
      r = 2;
    }

   if (l == 0 && currentMillis - previousMillis >= (ds + r_time + s_time)) {
     Serial.println(currentMillis);
     onRelay(RRELAY);
     l = 1;
   }
   if (l == 1 && currentMillis - previousMillis >= (ds + r_time*2+s_time)) {
      Serial.println(currentMillis);
      digitalWrite(RRELAY,LOW);
      Serial.println(" = R OFF");
      previousMillis = currentMillis;
      r = 0;
      l = 0;
    }
}


// Main ==================================================
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 - OLED Display");
  pinMode(OLEDRST,OUTPUT);
  pinMode(LRELAY,OUTPUT);
  pinMode(RRELAY,OUTPUT);
  pinMode(MRELAY,OUTPUT);
  pinMode(MODE,INPUT);

  digitalWrite(LRELAY,LOW);   
  digitalWrite(RRELAY,LOW);   
  digitalWrite(MRELAY,LOW);   
      
  digitalWrite(OLEDRST, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(OLEDRST, HIGH);   // while OLED is running, must set GPIO16 in hi
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Open_Sans_Condensed_Light_20); // set a font
  
  // Read settings
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        Serial.println("SPIFFS Mount Failed");
        return;
  }

  //deleteFile(SPIFFS, "/rotate.txt");deleteFile(SPIFFS, "/stop.txt");deleteFile(SPIFFS, "/run.txt");
  r_time  = getSPIFF("/rotate.txt", rotate_time); 
  s_time  = getSPIFF("/stop.txt", stop_time); 
  u_time  = getSPIFF("/run.txt", run_time); 
  Serial.println("-------------------");Serial.println(r_time);Serial.println(s_time);Serial.println(u_time);
  
  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
  // Security Settings
  BLESecurity *thingsSecurity = new BLESecurity();
  thingsSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  thingsSecurity->setCapability(ESP_IO_CAP_NONE);
  thingsSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  setupServices();
  startAdvertising();
  
  sensors.begin();
}

// Loop ===============================================
void loop() {
  char buf[100];
  auto_manual = digitalRead(MODE);
  if (auto_manual == 1) {
    runMotor(); 
  }
  else {
    if (cmd == 1) {
      runMotor();
      timeout++;
    }
  }

  sensors.requestTemperatures(); // Send the command to get temperatures
  temperature = sensors.getTempCByIndex(0);
  displayData(temperature);
  
  relaystatus =  (digitalRead(LRELAY) == HIGH || digitalRead(RRELAY) == HIGH) ? 1:0;
  
  sprintf(buf,"%0.2f,%d,%d,%d,%d,%d,%d", temperature,timeout,relaystatus,auto_manual,r_time,s_time,u_time);  
  notifyCharacteristic->setValue(buf);
  notifyCharacteristic->notify();
  
  // Disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Wait for BLE Stack to be ready
    thingsServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
    Serial.println("Disconnection & restart advertising");
  }
  // Connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    Serial.println("connected");
  }
}

void displayData(float t) 
{
  display.clear();   // clear the display
  display.drawString(0, 0,  "Temp: ");
  display.drawString(40, 0,  String(t)+"C");  //  display.drawString(40, 0,  String(localTemp));
  if (auto_manual == 0) {
    display.drawString(0, 32, "Time:  "); //  display.drawString(40, 32,  String(localHum));
    display.drawString(40, 32,  String(timeout));
  }
  else {
    display.drawString(0, 32, "** Manaul Mode **");    
  }
 
  display.display();   // write the buffer to the display
  delay(10);
}
