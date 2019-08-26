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

#define FORMAT_SPIFFS_IF_FAILED true
#define LRELAY 13
#define RRELAY 12
#define MRELAY 14
#define MODE 26

#define DS18PIN 27
OneWire oneWire(DS18PIN);
DallasTemperature sensors(&oneWire);

// ===== Configuration ============================
int auto_manual = 1;        // 0 = auto, 1 = manual 
char r_rotate_time[] = "500"; // rotation left time in ms
char l_rotate_time[] = "500"; // rotation right time in ms
char stop_time[]   = "1000";  // stop time in ms
char no_shake[]    = "20";    // no of shake
char settemp[] = "-7";          // set point 
unsigned int r_time,l_time,s_time,n_shake; // settings
float set_temp;
unsigned int relaystatus = 0;
unsigned int cmd = 0;         // command from LIFF default stop
float temperature;
int countdown = 0;            // count down shake
//unsigned long previousMillis = 0;        // will store last time LED was updated
// ================================================

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

    while(file.available()) {
      line = file.readStringUntil('\n');
    }
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
    Serial.print("data init = ");
    Serial.println(val);
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
    
    // retrieve configuration for LIFF and set global variables
    String ls = getValue(str, ',', 0);   // left rotate
    l_time = ls.toInt();
    String rs = getValue(str, ',', 1);   // right rotate
    r_time = rs.toInt();
    String ss = getValue(str, ',', 2);   // stop time
    s_time = ss.toInt();
    String ns = getValue(str, ',', 3);   // no shake
    n_shake = ns.toInt();
    String ts = getValue(str, ',', 4);   // set temp
    set_temp = ts.toFloat();
    String cs = getValue(str, ',', 5);   // command start/stop from LIFF
    cmd = cs.toInt();
   
    // write to spiffs
    char buf[100];
    // left rotate
    ls.toCharArray(buf,ls.length() + 1);
    writeFile(SPIFFS, "/left_rotate.txt", buf);
    Serial.print("left = ");Serial.println(buf);
    
    // right rotate
    rs.toCharArray(buf,rs.length() + 1);
    writeFile(SPIFFS, "/right_rotate.txt", buf);
    Serial.print("right = ");Serial.println(buf);

    // stop time
    ss.toCharArray(buf,ss.length() + 1);
    writeFile(SPIFFS, "/stop.txt", buf);
    Serial.print("stop = ");Serial.println(buf);

    // no shake
    ns.toCharArray(buf,ns.length() + 1);
    writeFile(SPIFFS, "/shake.txt", buf);
    Serial.print("shake = ");Serial.println(buf);

    // set temperature
    ts.toCharArray(buf,ts.length() + 1);
    writeFile(SPIFFS, "/settemp.txt", buf);
    Serial.print("settemp = ");Serial.println(buf);
}
  
// function split char
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

void onRelay(int n, int delaytime) {
  if (n == RRELAY) {
    if (digitalRead(LRELAY) == LOW) {
      digitalWrite(RRELAY,HIGH); 
      delay(delaytime);
      digitalWrite(RRELAY,LOW); 
    }
  }
  else if (n == LRELAY) {
    if (digitalRead(RRELAY) == LOW) {
      digitalWrite(LRELAY,HIGH);   
      delay(delaytime);
      digitalWrite(LRELAY,LOW);   
    }    
  }
}

void runMotor() {
  onRelay(LRELAY,r_time);
  delay(s_time);
  onRelay(RRELAY,l_time);  
  delay(s_time);
}


// Main ==================================================
void setup() {
  Serial.begin(115200);
  pinMode(OLEDRST,OUTPUT);
  pinMode(LRELAY,OUTPUT);
  pinMode(RRELAY,OUTPUT);
  pinMode(MRELAY,OUTPUT);
  pinMode(MODE,INPUT);

  // set all relays to low
  digitalWrite(LRELAY,LOW);   
  digitalWrite(RRELAY,LOW);   
  digitalWrite(MRELAY,LOW);   

  // init oled 
  digitalWrite(OLEDRST, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(OLEDRST, HIGH);   // while OLED is running, must set GPIO16 in hi
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Open_Sans_Condensed_Light_20); // set a font
  
  Serial.println("-- ICE Shaker v1.0 ---");
  
  // Read settings
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        Serial.println("SPIFFS Mount Failed");
        return;
  }

  // -- uncomment to delete all files --
  //deleteFile(SPIFFS, "/rotate.txt");deleteFile(SPIFFS, "/stop.txt");deleteFile(SPIFFS, "/run.txt");
  
  // get configuration from spiff
  l_time = getSPIFF("/left_rotate.txt", l_rotate_time); 
  r_time = getSPIFF("/right_rotate.txt", r_rotate_time); 
  s_time = getSPIFF("/stop.txt", stop_time); 
  n_shake = getSPIFF("/shake.txt", no_shake);
  set_temp = getSPIFF("/settemp.txt", settemp);
  Serial.println("- Initial value -");
  Serial.print("left rotation = "); Serial.println(l_time);
  Serial.print("right rotation = "); Serial.println(r_time);
  Serial.print("stop time = ");Serial.println(s_time);
  Serial.print("shake = ");Serial.println(n_shake);
  Serial.print("set temp. = ");Serial.println(set_temp);

  //-- Line Things setup
  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
  BLESecurity *thingsSecurity = new BLESecurity();
  thingsSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  thingsSecurity->setCapability(ESP_IO_CAP_NONE);
  thingsSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  setupServices();
  startAdvertising();

  //-- temperature sensor
  sensors.begin();
}

//== OLED display text 
void displayData(float t)  {
  display.clear();                            // clear the display
  display.drawString(0, 0,  "Temp: ");
  display.drawString(43, 0,  String(t)+" C"); 
  if (auto_manual == 0) {
    if (countdown > 0) {
      display.drawString(0, 32, "Shake:  ");     ;
      display.drawString(43, 32,  String(countdown));
    }
    else {
      display.drawString(0, 32, "** Auto Mode **");        
    }
  }
  else {
    display.drawString(0, 32, "** Manaul Mode **");    
  }
 
  display.display();                            // write the buffer to the display
  delay(10);
}

// Loop ===============================================
void loop() {
  char buf[100];
  
  auto_manual = digitalRead(MODE); // select mode
  // Read temperature
  sensors.requestTemperatures(); 
  temperature = sensors.getTempCByIndex(0);
  displayData(temperature);
  
  if (auto_manual == 1) { // Manual Mode
    runMotor(); 
  }
  else {                  // Auto Mode
    if (cmd == 1) {       // if Start command
      if (countdown == 0 ) {
        countdown =  n_shake;
      }
      else {             // stop
        if (temperature > set_temp) {
          runMotor();  
          countdown--;
          if (countdown == 0) { // stop when countdown to zero
            cmd = 0;
          }
        }
      }
    }
    else {
      cmd = 0;
      countdown = 0;
      digitalWrite(LRELAY,LOW);
      digitalWrite(RRELAY,LOW);
    }
  }

  // Send status to Line Things
  relaystatus =  (digitalRead(LRELAY) == HIGH || digitalRead(RRELAY) == HIGH) ? 1:0;
  sprintf(buf,"%0.2f,%d,%d,%d,%d,%d,%d,%d,%0.2f", temperature, countdown, relaystatus, auto_manual, r_time, l_time, s_time, n_shake,set_temp);  
  Serial.printf("%0.2f,%d,%d,%d,%d,%d,%d,%d,%0.2f", temperature, countdown, relaystatus, auto_manual, r_time, l_time, s_time, n_shake,set_temp);  
  
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
    Serial.println("Line connected");
  }
}
