#include "modified_font.h"
#include "SSD1306.h" 
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define SDA_PIN 4// GPIO21 -> SDA
#define SCL_PIN 15// GPIO22 -> SCL
#define SSD_ADDRESS 0x3c
SSD1306  display(SSD_ADDRESS, SDA_PIN, SCL_PIN);


#define DEVICE_NAME "Ice Shaker"

#define USER_SERVICE_UUID "5f961675-3050-4cef-8a98-2e7f58fb4e43"

#define WRITE_CHARACTERISTIC_UUID "E9062E71-9E62-4BC6-B0D3-35CDCD9B027B"
#define NOTIFY_CHARACTERISTIC_UUID "62FBD229-6EDD-4D1A-B554-5C4E1BB29169"

#define PSDI_SERVICE_UUID "e625601e-9e55-4597-a598-76018a0d293d"
#define PSDI_CHARACTERISTIC_UUID "26e2b12b-85f0-4f3f-9fdd-91d114270e6e"

#define RELAY 2
#define ON 0
#define OFF 1

int timeout=0;
unsigned int low=60;
unsigned int high=70;
unsigned int cmd=0;

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
    
    String ts = getValue(str, ',', 0);
    timeout = ts.toInt();
    String tl = getValue(str, ',', 1);
    low = tl.toInt();
    String th = getValue(str, ',', 2);
    high = th.toInt();
    String c = getValue(str, ',', 3);
    cmd = c.toInt();
    
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


void setup() {
  pinMode(16,OUTPUT);
  pinMode(2,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH);   // while OLED is running, must set GPIO16 in hi
  Serial.begin(115200);
  Serial.println("ESP32 - OLED Display");
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Open_Sans_Condensed_Light_20); // set a font
  
  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);

  // Security Settings
  BLESecurity *thingsSecurity = new BLESecurity();
  thingsSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  thingsSecurity->setCapability(ESP_IO_CAP_NONE);
  thingsSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  setupServices();
  startAdvertising();
}

void loop() 
{
  displayData();
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

   delay(500);
  }


/***************************************************
* Display Data
****************************************************/
void displayData() 
{
  display.clear();   // clear the display
  display.drawString(0, 0,  "Temp: ");
  display.drawString(40, 0,  " 25.6C");  //  display.drawString(40, 0,  String(localTemp));
  display.drawString(0, 32, "Time:  "); //  display.drawString(40, 32,  String(localHum));
  display.drawString(40, 32,  "2304");
  display.display();   // write the buffer to the display
  delay(10);
}


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
