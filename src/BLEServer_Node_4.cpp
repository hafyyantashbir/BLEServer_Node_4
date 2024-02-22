#include <Arduino.h>

//library RTC
#include <Wire.h>
#include "RTClib.h"

//library Json
#include <ArduinoJson.h>

//library NRF24L01
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#include "printf.h"

//library BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertisedDevice.h>

//konfigurasi stack size
SET_LOOP_TASK_STACK_SIZE(64*1024); // 64KB

//konfigurasi RTC
RTC_DS3231 rtc;
char days[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//konfigurasi BLE
#define SERVICE_UUID "17d5d798-6ecc-44b1-a535-c9c589fb5f64"
#define CHARACTERISTIC_UUID "c3d4bc8d-a7c5-48f7-8a40-8f9a47617581"

//konfigurasi NRF24L01
RF24 radio(4, 5); //(pin CE, pin CSN)
RF24Network network(radio);      // Network uses that radio
RF24Mesh mesh(radio, network);
uint8_t dataBuffer[MAX_PAYLOAD_SIZE];  //MAX_PAYLOAD_SIZE is defined in RF24Network_config.h
#define LED_BUILTIN 2

//alamat node
#define NODE_Master 0
#define this_node 4
#define NODE_1 1
#define NODE_2 2
#define NODE_3 3
#define NODE_5 5

//variabel DATA
int node_asal = 4;
unsigned long TofX = 5; // data TofX
unsigned long TofY = 10; // data TofY
unsigned long TofZ = 15; // data TofZ
String datakirim;
String dataterima;
int count = 0;
int jumlahnode[5];

//variabel new data
bool newdata = false;

//struktur daya yang dikirmkan
struct payload_t {  // Structure of our payload
  String datakirim;
};

//variabel millis
unsigned long previousTime = 0; // Waktu sebelumnya
unsigned long intervalmillis = 10000; // Interval waktu (dalam milidetik)

//variabel RSSI node
int NODE_Master_RSSI = -999;
int NODE_1_RSSI = -999;
int NODE_2_RSSI = -999;
int NODE_3_RSSI = -999;
int NODE_5_RSSI = -999;

//Status node mati/nyala
bool NODE_1_STATUS = false;
bool NODE_2_STATUS = false;
bool NODE_3_STATUS = false;
bool NODE_5_STATUS = false;

//variabel BLE
int scanTime = 1; //In seconds

//Fungsi untuk 2 loop
//TaskHandle_t Task1;

//konfigurasi fungsi scan RSSI BLE
BLEScan* pBLEScan;
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.getName() == "NODE_Master")
      {
        NODE_Master_RSSI = advertisedDevice.getRSSI();
      }
      if (advertisedDevice.getName() == "NODE_1")
      {
        NODE_1_RSSI = advertisedDevice.getRSSI();
      }
      if (advertisedDevice.getName() == "NODE_2")
      {
        NODE_2_RSSI = advertisedDevice.getRSSI();
      }
      if (advertisedDevice.getName() == "NODE_3")
      {
        NODE_3_RSSI = advertisedDevice.getRSSI();
      }
      if (advertisedDevice.getName() == "NODE_5")
      {
        NODE_5_RSSI = advertisedDevice.getRSSI();
      }
      //Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};

//program loop 2
//void loop2( void * parameter) {
//  for (;;) {
//    unsigned long currentTime = millis(); // Waktu saat ini
//
//    if (currentTime - previousTime >= intervalmillis) {
//      previousTime = currentTime; // Perbarui waktu sebelumnya
//      Serial.println("MODE : SCANNING......");
//      BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
//      Serial.print("RSSI NODE 1 : " + String(NODE_1_RSSI));
//      Serial.println(" || RSSI NODE 2 : " + String(NODE_2_RSSI));
//    }
//  }
//}

void setup() {
  Serial.begin(115200);
  StaticJsonDocument<512> doc; // buat document Json

  //Fungsi untuk 2 loop
  //  xTaskCreatePinnedToCore(
  //    loop2,
  //    "BLE_SCANNING",
  //    1000,
  //    NULL,
  //    1,
  //    &Task1,
  //    0);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  if (! rtc.begin()) {
    Serial.println("Could not find RTC! Check circuit.");
    while (1);
  }

  while (!Serial) {
    // some boards need this because of native USB capability
  }
  mesh.setNodeID(this_node); //Set the Node ID
  Serial.println(F("Connecting to the mesh..."));

  if (!mesh.begin()){
    if (radio.isChipConnected()){
      do {
        // mesh.renewAddress() will return MESH_DEFAULT_ADDRESS on failure to connect
        Serial.println(F("Could not connect to network.\nConnecting to the mesh..."));
      } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
    } else {
      Serial.println(F("Radio hardware not responding."));
      while (1) {
        // hold in an infinite loop
      }
    }
  }
  radio.startListening();
  printf_begin();        // needed for RF24* libs' internal printf() calls
  radio.printDetails();  // requires printf support

  //fungsi setup BLE
  BLEDevice::init("NODE_4");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer ->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristic->setValue("Hello this is NODE 4");
  pService->start();
  BLEDevice::startAdvertising();
  Serial.println("BLE READY!!!");

  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  //scan BLE
  Serial.println("SCANNING......");
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  Serial.print("RSSI NODE Master : " + String(NODE_Master_RSSI));
  Serial.print(" || RSSI NODE 1 : " + String(NODE_1_RSSI));
  Serial.print(" || RSSI NODE 2 : " + String(NODE_2_RSSI));
  Serial.print(" || RSSI NODE 3 : " + String(NODE_3_RSSI));
  Serial.println(" || RSSI NODE 5 : " + String(NODE_5_RSSI));

  Serial.printf("Arduino Stack was set to %d bytes", getArduinoLoopTaskStackSize());
  // Print unused stack for the task that is running setup()
  Serial.printf("\nSetup() - Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));
}

void loop() {
  // Print unused stack for the task that is running loop() - the same as for setup()
  Serial.printf("\nLoop() - Free Stack Space: %d", uxTaskGetStackHighWaterMark(NULL));
  
  mesh.update();  // Check the network regularly
  DateTime now = rtc.now();
  StaticJsonDocument<512> doc;

  //scan ble
  unsigned long currentTime = millis(); // Waktu saat ini

  // if (currentTime - previousTime >= intervalmillis) {
  //   previousTime = currentTime; // Perbarui waktu sebelumnya
  //   Serial.println("MODE : SCANNING......");
  //   BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  //   Serial.print("RSSI NODE 1 : " + String(NODE_1_RSSI));
  //   Serial.print(" || RSSI NODE 2 : " + String(NODE_2_RSSI));
  //   Serial.print(" || RSSI NODE 3 : " + String(NODE_3_RSSI));
  //   Serial.println(" || RSSI NODE 5 : " + String(NODE_5_RSSI));
  // }
  
  while (network.available()) {
    RF24NetworkHeader header;  // If so, grab it and print it out
    uint16_t payloadSize = network.peek(header);     // Use peek() to get the size of the payload
    char terima_loop[payloadSize]; //buat variabel untuk nerima data array
    network.read(header, &terima_loop, payloadSize);
    dataterima = "";
    for (uint32_t i = 0; i < payloadSize; i++){
      dataterima += terima_loop[i];
    }
    Serial.println(dataterima);
    DeserializationError error = deserializeJson(doc, dataterima);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    for (int i = 0; i <= 4; i++) {
      jumlahnode[i] = NULL;
      count = 0;
    }
    for (int i = 0; i <= 4; i++) {
      JsonObject parsing = doc[i];
      int NodeID = parsing["NodeID"];
      if (NodeID != 0) {
        count++;
        jumlahnode[i] += NodeID;
      }
    }
    newdata = true;
  }
  if (newdata == true){
    radio.stopListening();
    NODE_1_RSSI = -999;
    NODE_2_RSSI = -999;
    NODE_3_RSSI = -999;
    NODE_5_RSSI = -999;
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    if(NODE_1_RSSI != -999){
      NODE_1_STATUS = true;
    }
    if(NODE_2_RSSI != -999){
      NODE_2_STATUS = true;
    }
    if(NODE_3_RSSI != -999){
      NODE_3_STATUS = true;
    }
    if(NODE_5_RSSI != -999){
      NODE_5_STATUS = true;
    }
    //==================================================POSISI NODE KE - 1==================================================
    if (count == 1 && jumlahnode[0] == 6) {
      Serial.print("Received packet from NODE Master");
      JsonArray jsonarray = doc.to<JsonArray>();
      JsonObject jsonobject = jsonarray.createNestedObject();
      jsonobject["NodeID"] = node_asal;
      jsonobject["TofX"] = TofX;
      jsonobject["TofY"] = TofY;
      jsonobject["TofZ"] = TofZ;
      jsonobject["Unixtime"] = now.unixtime();
      datakirim = "";
      serializeJson(doc, datakirim);
      char kirim_loop[datakirim.length() + 1];
      datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
      if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_2_RSSI && NODE_1_RSSI >= NODE_3_RSSI && NODE_1_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI && NODE_2_RSSI >= NODE_3_RSSI && NODE_2_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI && NODE_3_RSSI >= NODE_2_RSSI && NODE_3_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI && NODE_5_RSSI >= NODE_2_RSSI && NODE_5_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if (NODE_1_RSSI >= NODE_2_RSSI && NODE_1_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI && NODE_2_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI && NODE_3_RSSI >= NODE_2_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_2_RSSI && NODE_1_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI && NODE_2_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI && NODE_5_RSSI >= NODE_2_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if (NODE_1_RSSI >= NODE_2_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_3_RSSI && NODE_1_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI && NODE_3_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI && NODE_5_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if (NODE_1_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_2_RSSI >= NODE_3_RSSI && NODE_2_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_2_RSSI && NODE_3_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_2_RSSI && NODE_5_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if (NODE_2_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_2_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if (NODE_2_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_2_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_3_RSSI >= NODE_5_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_3_RSSI){
          if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if(mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          newdata = false;
        }
      }
    }
    //==================================================POSISI NODE KE - 2==================================================
    if (count == 1 && jumlahnode[0] == 1) {
      Serial.print("Received packet from NODE 1");
      JsonObject NodeID_1 = doc[0];
      int aNodeID_1 = NodeID_1["NodeID"];
      int Suhu = NodeID_1["Suhu"];
      int Kelembapan = NodeID_1["Kelembapan"];
      uint32_t Unixtime = NodeID_1["Unixtime"];
      JsonArray jsonarray = doc.to<JsonArray>();
      JsonObject jsonobject = jsonarray.createNestedObject();
      jsonobject["NodeID"] = aNodeID_1;
      jsonobject["Suhu"] = Suhu;
      jsonobject["Kelembapan"] = Kelembapan;
      jsonobject["Unixtime"] = Unixtime;
      JsonObject jsonobject1 = jsonarray.createNestedObject();
      jsonobject1["NodeID"] = node_asal;
      jsonobject1["TofX"] = TofX;
      jsonobject1["TofY"] = TofY;
      jsonobject1["TofZ"] = TofZ;
      jsonobject1["Unixtime"] = now.unixtime();
      datakirim = "";
      serializeJson(doc, datakirim);
      char kirim_loop[datakirim.length() + 1];
      datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
      if(NODE_2_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_2_RSSI >= NODE_3_RSSI && NODE_2_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_2_RSSI && NODE_3_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_2_RSSI && NODE_5_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_2_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if (NODE_2_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_2_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if (NODE_2_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_2_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
          newdata = false;
        }
      }else if(NODE_2_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_3_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_2_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
          newdata = false;
        }
      }else if(NODE_2_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
          newdata = false;
        }
      }else if(NODE_2_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          newdata = false;
        }
      }
    }
    if (count == 1 && jumlahnode[0] == 2) {
      Serial.print("Received packet from NODE 2");
      JsonObject NodeID_2 = doc[0];
      int aNodeID_2 = NodeID_2["NodeID"];
      int Berat = NodeID_2["Berat"];
      uint32_t aUnixtime = NodeID_2["Unixtime"];
      JsonArray jsonarray = doc.to<JsonArray>();
      JsonObject jsonobject = jsonarray.createNestedObject();
      jsonobject["NodeID"] = aNodeID_2;
      jsonobject["Berat"] = Berat;
      jsonobject["Unixtime"] = aUnixtime;
      JsonObject jsonobject1 = jsonarray.createNestedObject();
      jsonobject1["NodeID"] = node_asal;
      jsonobject1["TofX"] = TofX;
      jsonobject1["TofY"] = TofY;
      jsonobject1["TofZ"] = TofZ;
      jsonobject1["Unixtime"] = now.unixtime();
      datakirim = "";
      serializeJson(doc, datakirim);
      char kirim_loop[datakirim.length() + 1];
      datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
      if(NODE_1_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_3_RSSI && NODE_1_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI && NODE_3_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI && NODE_5_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if (NODE_1_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == true){
        if (NODE_3_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == false && NODE_3_STATUS == true && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == true){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_3_STATUS == false && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          newdata = false;
        }
      }
    }
    if (count == 1 && jumlahnode[0] == 3) {
      Serial.print("Received packet from NODE 3");
      JsonObject NodeID_3 = doc[0];
      int aNodeID_3 = NodeID_3["NodeID"];
      int Pitch = NodeID_3["Pitch"];
      int Roll = NodeID_3["Roll"];
      int Frekuensi = NodeID_3["Frekuensi"];
      uint32_t bUnixtime = NodeID_3["unixtime"];
      JsonArray jsonarray = doc.to<JsonArray>();
      JsonObject jsonobject = jsonarray.createNestedObject();
      jsonobject["NodeID"] = aNodeID_3;
      jsonobject["Pitch"] = Pitch;
      jsonobject["Roll"] = Roll;
      jsonobject["Frekuensi"] = Frekuensi;
      jsonobject["Unixtime"] = bUnixtime;
      JsonObject jsonobject1 = jsonarray.createNestedObject();
      jsonobject1["NodeID"] = node_asal;
      jsonobject1["TofX"] = TofX;
      jsonobject1["TofY"] = TofY;
      jsonobject1["TofZ"] = TofZ;
      jsonobject1["Unixtime"] = now.unixtime();
      datakirim = "";
      serializeJson(doc, datakirim);
      char kirim_loop[datakirim.length() + 1];
      datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
      if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_2_RSSI && NODE_1_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI && NODE_2_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI && NODE_5_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_5_STATUS == false){
        if (NODE_1_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_5_STATUS == true){
        if (NODE_1_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_1_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_5_STATUS == true){
        if (NODE_2_RSSI >= NODE_5_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_5_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_5_STATUS == true){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_5_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          newdata = false;
        }
      }
    }
    if (count == 1 && jumlahnode[0] == 5) {
      Serial.print("Received packet from NODE 5");
      JsonObject NodeID_5 = doc[0];
      int aNodeID_5 = NodeID_5["NodeID"];
      int usX = NodeID_5["usX"];
      int usY = NodeID_5["usY"];
      int usZ = NodeID_5["usZ"];
      uint32_t bUnixtime = NodeID_5["unixtime"];
      JsonArray jsonarray = doc.to<JsonArray>();
      JsonObject jsonobject = jsonarray.createNestedObject();
      jsonobject["NodeID"] = aNodeID_5;
      jsonobject["usX"] = usX;
      jsonobject["usY"] = usY;
      jsonobject["usZ"] = usZ;
      jsonobject["Unixtime"] = bUnixtime;
      JsonObject jsonobject1 = jsonarray.createNestedObject();
      jsonobject1["NodeID"] = node_asal;
      jsonobject1["TofX"] = TofX;
      jsonobject1["TofY"] = TofY;
      jsonobject1["TofZ"] = TofZ;
      jsonobject1["Unixtime"] = now.unixtime();
      datakirim = "";
      serializeJson(doc, datakirim);
      char kirim_loop[datakirim.length() + 1];
      datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
      if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_3_STATUS == true){
        if (NODE_1_RSSI >= NODE_2_RSSI && NODE_1_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI && NODE_2_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI && NODE_3_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == true && NODE_3_STATUS == false){
        if (NODE_1_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_2_RSSI >= NODE_1_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_3_STATUS == true){
        if (NODE_1_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_1_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == true && NODE_2_STATUS == false && NODE_3_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_3_STATUS == true){
        if (NODE_2_RSSI >= NODE_3_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if (NODE_3_RSSI >= NODE_2_RSSI){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == true && NODE_3_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_3_STATUS == true){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
          newdata = false;
        }
      }else if(NODE_1_STATUS == false && NODE_2_STATUS == false && NODE_3_STATUS == false){
        if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          newdata = false;
        }
      }
    }
    //==================================================POSISI NODE KE - 3==================================================
    if (count == 2) {
      Serial.print("Received packet from NODE " + jumlahnode[1]);
      if (jumlahnode[0] == 1 && jumlahnode[1] == 2) {
        JsonObject NodeID_1 = doc[0];
        int bNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t cUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int bNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t dUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = bNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = cUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = bNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = dUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_3_STATUS == true && NODE_5_STATUS == true){
          if (NODE_3_RSSI >= NODE_5_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
              newdata = false;
            }
          }else if (NODE_5_RSSI >= NODE_3_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
              newdata = false;
            }
          }
        }else if(NODE_3_STATUS == true && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if(NODE_3_STATUS == false && NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else if(NODE_3_STATUS == false && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 1 && jumlahnode[1] == 3) {
        JsonObject NodeID_1 = doc[0];
        int dNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t gUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int bNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t hUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = dNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = gUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = bNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = hUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_2_STATUS == true && NODE_5_STATUS == true){
          if (NODE_2_RSSI >= NODE_5_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
              newdata = false;
            }
          }else if (NODE_5_RSSI >= NODE_2_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
              newdata = false;
            }
          }
        }else if(NODE_2_STATUS == true && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 1 && jumlahnode[1] == 5) {
        JsonObject NodeID_1 = doc[0];
        int ddNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t gUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int bNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t hUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = ddNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = gUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = bNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = hUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_2_STATUS == true && NODE_3_STATUS == true){
          if (NODE_2_RSSI >= NODE_3_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
              newdata = false;
            }
          }else if (NODE_3_RSSI >= NODE_2_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
              newdata = false;
            }
          }
        }else if(NODE_2_STATUS == true && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 2 && jumlahnode[1] == 1) {
        JsonObject NodeID_2 = doc[0];
        int cNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t eUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int cNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t fUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = cNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = eUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = cNodeID_1;
        jsonobject1["Suhu"] = Suhu;;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = fUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_3_STATUS == true && NODE_5_STATUS == true){
          if (NODE_3_RSSI >= NODE_5_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
              newdata = false;
            }
          }else if (NODE_5_RSSI >= NODE_3_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
              newdata = false;
            }
          }
        }else if(NODE_3_STATUS == true && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if(NODE_3_STATUS == false && NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else if(NODE_3_STATUS == false && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 2 && jumlahnode[1] == 3) {
        JsonObject NodeID_2 = doc[0];
        int eNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t mUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int eNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t nUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = eNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = mUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = eNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = nUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_1_STATUS == true && NODE_5_STATUS == true){
          if (NODE_1_RSSI >= NODE_5_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
              newdata = false;
            }
          }else if (NODE_5_RSSI >= NODE_1_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
              newdata = false;
            }
          }
        }else if(NODE_1_STATUS == true && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 2 && jumlahnode[1] == 5) {
        JsonObject NodeID_2 = doc[0];
        int eeNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t mmUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int eNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t nnUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = eeNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = mmUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = eNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = nnUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_1_STATUS == true && NODE_3_STATUS == true){
          if (NODE_1_RSSI >= NODE_3_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
              newdata = false;
            }
          }else if (NODE_3_RSSI >= NODE_1_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
              newdata = false;
            }
          }
        }else if(NODE_1_STATUS == true && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 3 && jumlahnode[1] == 1) {
        JsonObject NodeID_3 = doc[0];
        int xxNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t xxUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int xxNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t llUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = xxNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = xxUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = xxNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = llUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_2_STATUS == true && NODE_5_STATUS == true){
          if (NODE_2_RSSI >= NODE_5_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
              newdata = false;
            }
          }else if (NODE_5_RSSI >= NODE_2_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
              newdata = false;
            }
          }
        }else if(NODE_2_STATUS == true && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 3 && jumlahnode[1] == 2) {
        JsonObject NodeID_3 = doc[0];
        int dNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t kUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int dNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t lUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = dNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = kUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = dNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = lUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_1_STATUS == true && NODE_5_STATUS == true){
          if (NODE_1_RSSI >= NODE_5_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
              newdata = false;
            }
          }else if (NODE_5_RSSI >= NODE_1_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
              newdata = false;
            }
          }
        }else if(NODE_1_STATUS == true && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_5_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 3 && jumlahnode[1] == 5) {
        JsonObject NodeID_3 = doc[0];
        int ddNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t kkUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int dNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t llUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = ddNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = kkUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = dNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = llUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_1_STATUS == true && NODE_2_STATUS == true){
          if (NODE_1_RSSI >= NODE_2_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
              newdata = false;
            }
          }else if (NODE_2_RSSI >= NODE_1_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
              newdata = false;
            }
          }
        }else if(NODE_1_STATUS == true && NODE_2_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_2_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 5 && jumlahnode[1] == 1) {
        JsonObject NodeID_5 = doc[0];
        int eNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t oUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int eNodeID_3 = NodeID_3["NodeID"];
        int Suhu = NodeID_3["Suhu"];
        int Kelembapan = NodeID_3["Kelembapan"];
        uint32_t pUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = eNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = eNodeID_3;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_2_STATUS == true && NODE_3_STATUS == true){
          if (NODE_2_RSSI >= NODE_3_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
              newdata = false;
            }
          }else if (NODE_3_RSSI >= NODE_2_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
              newdata = false;
            }
          }
        }else if(NODE_2_STATUS == true && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if(NODE_2_STATUS == false && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 5 && jumlahnode[1] == 2) {
        JsonObject NodeID_5 = doc[0];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t qUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t rUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = qUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = rUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_1_STATUS == true && NODE_3_STATUS == true){
          if (NODE_1_RSSI >= NODE_3_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
              newdata = false;
            }
          }else if (NODE_3_RSSI >= NODE_1_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
              newdata = false;
            }
          }
        }else if(NODE_1_STATUS == true && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_3_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if (jumlahnode[0] == 5 && jumlahnode[1] == 3) {
        JsonObject NodeID_5 = doc[0];
        int gNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t sUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int gNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t tUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = gNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = sUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = gNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = tUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = node_asal;
        jsonobject2["TofX"] = TofX;
        jsonobject2["TofY"] = TofY;
        jsonobject2["TofZ"] = TofZ;
        jsonobject2["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if(NODE_1_STATUS == true && NODE_2_STATUS == true){
          if (NODE_1_RSSI >= NODE_2_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
              newdata = false;
            }
          }else if (NODE_2_RSSI >= NODE_1_RSSI){
            if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
              newdata = false;
            }
          }
        }else if(NODE_1_STATUS == true && NODE_2_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else if(NODE_1_STATUS == false && NODE_2_STATUS == false){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
    }
    //==================================================POSISI NODE KE - 4==================================================  
    if( count == 3){
      Serial.print("Received packet from NODE " + jumlahnode[2]);
      if(jumlahnode[0] == 1 && jumlahnode[1] == 2 && jumlahnode[2] == 3){
        JsonObject NodeID_1 = doc[0];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = qUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 2 && jumlahnode[2] == 5){
        JsonObject NodeID_1 = doc[0];
        int ffNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t ooUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int ffNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t ppUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int ffNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t qqUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = ffNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = ooUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = ffNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = ppUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = ffNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = qqUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 3 && jumlahnode[2] == 2){
        JsonObject NodeID_1 = doc[0];
        int gNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t rUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int gNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t sUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int gNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t tUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = gNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = gNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = sUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = gNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = tUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 3 && jumlahnode[2] == 5){
        JsonObject NodeID_1 = doc[0];
        int gNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t rUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int gNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t sUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int gNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t tUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = gNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = gNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = sUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = gNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = tUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 5 && jumlahnode[2] == 2){
        JsonObject NodeID_1 = doc[0];
        int gNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t rUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int gNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t tUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int gNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t sUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = gNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = gNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = tUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = gNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = sUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 5 && jumlahnode[2] == 3){
        JsonObject NodeID_1 = doc[0];
        int gNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t rUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int gNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t tUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int gNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t sUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = gNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = gNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = tUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = gNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = sUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 1 && jumlahnode[2] == 3){
        JsonObject NodeID_2 = doc[0];
        int hNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t uUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int hNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["kelembapan"];
        uint32_t vUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int hNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t wUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = hNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = uUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = hNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = vUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = hNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = wUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 1 && jumlahnode[2] == 5){
        JsonObject NodeID_2 = doc[0];
        int hNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t uUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int hNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["kelembapan"];
        uint32_t vUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int hNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t wUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = hNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = uUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = hNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = vUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = hNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = wUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 3 && jumlahnode[2] == 1){
        JsonObject NodeID_2 = doc[0];
        int iNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t xUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int iNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t yUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int iNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t zUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = iNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = xUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = iNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = yUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = iNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["unixtime"] = zUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 3 && jumlahnode[2] == 5){
        JsonObject NodeID_2 = doc[0];
        int iNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t xUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int iNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t yUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int iNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t zUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = iNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = xUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = iNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = yUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = iNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["unixtime"] = zUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_1_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 5 && jumlahnode[2] == 1){
        JsonObject NodeID_2 = doc[0];
        int iNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t xUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int iNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t zUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int iNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t yUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = iNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = xUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = iNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["unixtime"] = zUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = iNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = yUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 5 && jumlahnode[2] == 3){
        JsonObject NodeID_2 = doc[0];
        int iNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t xUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int iNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t zUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int iNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t yUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = iNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = xUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = iNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["unixtime"] = zUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = iNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = yUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_1_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 1 && jumlahnode[2] == 2){
        JsonObject NodeID_3 = doc[0];
        int kNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t adUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int kNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t aeUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int kNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t afUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = kNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = adUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = kNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = aeUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = kNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = afUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 1 && jumlahnode[2] == 5){
        JsonObject NodeID_3 = doc[0];
        int kNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t adUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int kNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t aeUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int kNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t afUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = kNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = adUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = kNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = aeUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = kNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = afUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 2 && jumlahnode[2] == 1){
        JsonObject NodeID_3 = doc[0];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t aaUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int jNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t abUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int jNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t acUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = aaUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = abUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = acUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_5_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_5)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 2 && jumlahnode[2] == 5){
        JsonObject NodeID_3 = doc[0];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t aaUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int jNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t abUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = aaUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = abUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = acUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_1_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 5 && jumlahnode[2] == 1){
        JsonObject NodeID_3 = doc[0];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t aaUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int jNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t abUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = aaUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = acUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = abUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 5 && jumlahnode[2] == 2){
        JsonObject NodeID_3 = doc[0];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t aaUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int jNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t abUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = aaUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = acUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = abUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_1_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 1 && jumlahnode[2] == 2){
        JsonObject NodeID_5 = doc[0];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int jNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t aaUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int jNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t abUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = acUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = aaUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = abUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 1 && jumlahnode[2] == 3){
        JsonObject NodeID_5 = doc[0];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int jNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t aaUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t abUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = acUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = aaUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = abUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 2 && jumlahnode[2] == 1){
        JsonObject NodeID_5 = doc[0];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int jNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t abUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int jNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t aaUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = acUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = abUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = aaUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_3_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_3)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 2 && jumlahnode[2] == 3){
        JsonObject NodeID_5 = doc[0];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int jNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t abUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t aaUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = acUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = abUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = aaUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if (NODE_1_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 3 && jumlahnode[2] == 1){
        JsonObject NodeID_5 = doc[0];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t abUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int jNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t aaUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = acUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = abUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = aaUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_2_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_2)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 3 && jumlahnode[2] == 2){
        JsonObject NodeID_5 = doc[0];
        int jNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t acUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int jNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t aaUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int jNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t abUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = jNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = acUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = jNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = aaUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = jNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = abUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = node_asal;
        jsonobject3["TofX"] = TofX;
        jsonobject3["TofY"] = TofY;
        jsonobject3["TofZ"] = TofZ;
        jsonobject3["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));
        if (NODE_1_STATUS == true){
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_1)){
            newdata = false;
          }
        }else{
          if (mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
            newdata = false;
          }
        }
      }
    }
    //==================================================POSISI NODE KE - 5==================================================  
    if(count == 4){
      Serial.print("Received packet from NODE " + jumlahnode[3]);
      if(jumlahnode[0] == 5 && jumlahnode[1] == 3 && jumlahnode[2] == 2 && jumlahnode[3] == 1){
        JsonObject NodeID_5 = doc[0];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[3];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = qUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = pUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_1;
        jsonobject3["Suhu"] = Suhu;
        jsonobject3["Kelembapan"] = Kelembapan;
        jsonobject3["Unixtime"] = oUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 3 && jumlahnode[2] == 1 && jumlahnode[3] == 2){
        JsonObject NodeID_5 = doc[0];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[3];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = qUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = oUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_2;
        jsonobject3["Berat"] = Berat;
        jsonobject3["Unixtime"] = pUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 2 && jumlahnode[2] == 3 && jumlahnode[3] == 1){
        JsonObject NodeID_5 = doc[0];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[3];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = qUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_1;
        jsonobject3["Suhu"] = Suhu;
        jsonobject3["Kelembapan"] = Kelembapan;
        jsonobject3["Unixtime"] = oUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 2 && jumlahnode[2] == 1 && jumlahnode[3] == 3){
        JsonObject NodeID_5 = doc[0];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[3];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = oUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_3;
        jsonobject3["Pitch"] = Pitch;
        jsonobject3["Roll"] = Roll;
        jsonobject3["Frekuensi"] = Frekuensi;
        jsonobject3["Unixtime"] = qUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 1 && jumlahnode[2] == 3 && jumlahnode[3] == 2){
        JsonObject NodeID_5 = doc[0];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[3];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = oUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = qUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_2;
        jsonobject3["Berat"] = Berat;
        jsonobject3["Unixtime"] = pUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 5 && jumlahnode[1] == 1 && jumlahnode[2] == 2 && jumlahnode[3] == 3){
        JsonObject NodeID_5 = doc[0];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[3];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_5;
        jsonobject["usX"] = usX;
        jsonobject["usY"] = usY;
        jsonobject["usZ"] = usZ;
        jsonobject["Unixtime"] = rUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = oUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = pUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_3;
        jsonobject3["Pitch"] = Pitch;
        jsonobject3["Roll"] = Roll;
        jsonobject3["Frekuensi"] = Frekuensi;
        jsonobject3["Unixtime"] = qUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 5 && jumlahnode[2] == 2 && jumlahnode[3] == 1){
        JsonObject NodeID_3 = doc[0];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[3];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = qUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = rUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = pUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_1;
        jsonobject3["Suhu"] = Suhu;
        jsonobject3["Kelembapan"] = Kelembapan;
        jsonobject3["Unixtime"] = oUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 5 && jumlahnode[2] == 1 && jumlahnode[3] == 2){
        JsonObject NodeID_3 = doc[0];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[3];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = qUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = rUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = oUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_2;
        jsonobject3["Berat"] = Berat;
        jsonobject3["Unixtime"] = pUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 2 && jumlahnode[2] == 5 && jumlahnode[3] == 1){
        JsonObject NodeID_3 = doc[0];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[3];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = qUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = rUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_1;
        jsonobject3["Suhu"] = Suhu;
        jsonobject3["Kelembapan"] = Kelembapan;
        jsonobject3["Unixtime"] = oUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 2 && jumlahnode[2] == 1 && jumlahnode[3] == 5){
        JsonObject NodeID_3 = doc[0];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[3];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = qUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = oUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_5;
        jsonobject3["usX"] = usX;
        jsonobject3["usY"] = usY;
        jsonobject3["usZ"] = usZ;
        jsonobject3["Unixtime"] = rUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 1 && jumlahnode[2] == 5 && jumlahnode[3] == 2){
        JsonObject NodeID_3 = doc[0];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[3];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = qUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = oUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = rUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_2;
        jsonobject3["Berat"] = Berat;
        jsonobject3["Unixtime"] = pUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 3 && jumlahnode[1] == 1 && jumlahnode[2] == 2 && jumlahnode[3] == 5){
        JsonObject NodeID_3 = doc[0];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[3];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_3;
        jsonobject["Pitch"] = Pitch;
        jsonobject["Roll"] = Roll;
        jsonobject["Frekuensi"] = Frekuensi;
        jsonobject["Unixtime"] = qUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = oUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = pUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_5;
        jsonobject3["usX"] = usX;
        jsonobject3["usY"] = usY;
        jsonobject3["usZ"] = usZ;
        jsonobject3["Unixtime"] = rUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 5 && jumlahnode[2] == 3 && jumlahnode[3] == 1){
        JsonObject NodeID_2 = doc[0];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[3];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = pUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = rUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = qUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_1;
        jsonobject3["Suhu"] = Suhu;
        jsonobject3["Kelembapan"] = Kelembapan;
        jsonobject3["Unixtime"] = oUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 5 && jumlahnode[2] == 1 && jumlahnode[3] == 3){
        JsonObject NodeID_2 = doc[0];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[3];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = pUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = rUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = oUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_3;
        jsonobject3["Pitch"] = Pitch;
        jsonobject3["Roll"] = Roll;
        jsonobject3["Frekuensi"] = Frekuensi;
        jsonobject3["Unixtime"] = qUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 3 && jumlahnode[2] == 5 && jumlahnode[3] == 1){
        JsonObject NodeID_2 = doc[0];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_1 = doc[3];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = pUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = qUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = rUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_1;
        jsonobject3["Suhu"] = Suhu;
        jsonobject3["Kelembapan"] = Kelembapan;
        jsonobject3["Unixtime"] = oUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 3 && jumlahnode[2] == 1 && jumlahnode[3] == 5){
        JsonObject NodeID_2 = doc[0];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_1 = doc[2];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[3];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = pUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = qUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_1;
        jsonobject2["Suhu"] = Suhu;
        jsonobject2["Kelembapan"] = Kelembapan;
        jsonobject2["Unixtime"] = oUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_5;
        jsonobject3["usX"] = usX;
        jsonobject3["usY"] = usY;
        jsonobject3["usZ"] = usZ;
        jsonobject3["Unixtime"] = rUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 1 && jumlahnode[2] == 5 && jumlahnode[3] == 3){
        JsonObject NodeID_2 = doc[0];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[3];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = pUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = oUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = rUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_3;
        jsonobject3["Pitch"] = Pitch;
        jsonobject3["Roll"] = Roll;
        jsonobject3["Frekuensi"] = Frekuensi;
        jsonobject3["Unixtime"] = qUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 2 && jumlahnode[1] == 1 && jumlahnode[2] == 3 && jumlahnode[3] == 5){
        JsonObject NodeID_2 = doc[0];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_1 = doc[1];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[3];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_2;
        jsonobject["Berat"] = Berat;
        jsonobject["Unixtime"] = pUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_1;
        jsonobject1["Suhu"] = Suhu;
        jsonobject1["Kelembapan"] = Kelembapan;
        jsonobject1["Unixtime"] = oUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = qUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_5;
        jsonobject3["usX"] = usX;
        jsonobject3["usY"] = usY;
        jsonobject3["usZ"] = usZ;
        jsonobject3["Unixtime"] = rUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 5 && jumlahnode[2] == 3 && jumlahnode[3] == 2){
        JsonObject NodeID_1 = doc[0];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[3];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = rUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = qUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_2;
        jsonobject3["Berat"] = Berat;
        jsonobject3["Unixtime"] = pUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 5 && jumlahnode[2] == 2 && jumlahnode[3] == 3){
        JsonObject NodeID_1 = doc[0];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_5 = doc[1];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[3];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_5;
        jsonobject1["usX"] = usX;
        jsonobject1["usY"] = usY;
        jsonobject1["usZ"] = usZ;
        jsonobject1["Unixtime"] = rUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = pUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_3;
        jsonobject3["Pitch"] = Pitch;
        jsonobject3["Roll"] = Roll;
        jsonobject3["Frekuensi"] = Frekuensi;
        jsonobject3["Unixtime"] = qUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 3 && jumlahnode[2] == 5 && jumlahnode[3] == 2){
        JsonObject NodeID_1 = doc[0];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_2 = doc[3];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = qUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = rUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_2;
        jsonobject3["Berat"] = Berat;
        jsonobject3["Unixtime"] = pUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 3 && jumlahnode[2] == 2 && jumlahnode[3] == 5){
        JsonObject NodeID_1 = doc[0];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_3 = doc[1];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_2 = doc[2];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[3];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_3;
        jsonobject1["Pitch"] = Pitch;
        jsonobject1["Roll"] = Roll;
        jsonobject1["Frekuensi"] = Frekuensi;
        jsonobject1["Unixtime"] = qUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_2;
        jsonobject2["Berat"] = Berat;
        jsonobject2["Unixtime"] = pUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_5;
        jsonobject3["usX"] = usX;
        jsonobject3["usY"] = usY;
        jsonobject3["usZ"] = usZ;
        jsonobject3["Unixtime"] = rUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 2 && jumlahnode[2] == 5 && jumlahnode[3] == 3){
        JsonObject NodeID_1 = doc[0];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_5 = doc[2];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonObject NodeID_3 = doc[3];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_5;
        jsonobject2["usX"] = usX;
        jsonobject2["usY"] = usY;
        jsonobject2["usZ"] = usZ;
        jsonobject2["Unixtime"] = rUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_3;
        jsonobject3["Pitch"] = Pitch;
        jsonobject3["Roll"] = Roll;
        jsonobject3["Frekuensi"] = Frekuensi;
        jsonobject3["Unixtime"] = qUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
      if(jumlahnode[0] == 1 && jumlahnode[1] == 2 && jumlahnode[2] == 3 && jumlahnode[3] == 5){
        JsonObject NodeID_1 = doc[0];
        int fNodeID_1 = NodeID_1["NodeID"];
        int Suhu = NodeID_1["Suhu"];
        int Kelembapan = NodeID_1["Kelembapan"];
        uint32_t oUnixtime = NodeID_1["Unixtime"];
        JsonObject NodeID_2 = doc[1];
        int fNodeID_2 = NodeID_2["NodeID"];
        int Berat = NodeID_2["Berat"];
        uint32_t pUnixtime = NodeID_2["Unixtime"];
        JsonObject NodeID_3 = doc[2];
        int fNodeID_3 = NodeID_3["NodeID"];
        int Pitch = NodeID_3["Pitch"];
        int Roll = NodeID_3["Roll"];
        int Frekuensi = NodeID_3["Frekuensi"];
        uint32_t qUnixtime = NodeID_3["Unixtime"];
        JsonObject NodeID_5 = doc[3];
        int fNodeID_5 = NodeID_5["NodeID"];
        int usX = NodeID_5["usX"];
        int usY = NodeID_5["usY"];
        int usZ = NodeID_5["usZ"];
        uint32_t rUnixtime = NodeID_5["Unixtime"];
        JsonArray jsonarray = doc.to<JsonArray>();
        JsonObject jsonobject = jsonarray.createNestedObject();
        jsonobject["NodeID"] = fNodeID_1;
        jsonobject["Suhu"] = Suhu;
        jsonobject["Kelembapan"] = Kelembapan;
        jsonobject["Unixtime"] = oUnixtime;
        JsonObject jsonobject1 = jsonarray.createNestedObject();
        jsonobject1["NodeID"] = fNodeID_2;
        jsonobject1["Berat"] = Berat;
        jsonobject1["Unixtime"] = pUnixtime;
        JsonObject jsonobject2 = jsonarray.createNestedObject();
        jsonobject2["NodeID"] = fNodeID_3;
        jsonobject2["Pitch"] = Pitch;
        jsonobject2["Roll"] = Roll;
        jsonobject2["Frekuensi"] = Frekuensi;
        jsonobject2["Unixtime"] = qUnixtime;
        JsonObject jsonobject3 = jsonarray.createNestedObject();
        jsonobject3["NodeID"] = fNodeID_5;
        jsonobject3["usX"] = usX;
        jsonobject3["usY"] = usY;
        jsonobject3["usZ"] = usZ;
        jsonobject3["Unixtime"] = rUnixtime;
        JsonObject jsonobject4 = jsonarray.createNestedObject();
        jsonobject4["NodeID"] = node_asal;
        jsonobject4["TofX"] = TofX;
        jsonobject4["TofY"] = TofY;
        jsonobject4["TofZ"] = TofZ;
        jsonobject4["Unixtime"] = now.unixtime();
        datakirim = "";
        serializeJson(doc, datakirim);
        char kirim_loop[datakirim.length() + 1];
        datakirim.toCharArray(kirim_loop,sizeof(kirim_loop));        
        if(!mesh.write(&kirim_loop, 'M', sizeof(kirim_loop), NODE_Master)){
          Serial.print(F("GAGAL TERKIRIM KE NODE MASTER"));
          if (!mesh.checkConnection()) {
            do {
              Serial.println(F("Reconnecting to mesh network..."));
            } while (mesh.renewAddress() == MESH_DEFAULT_ADDRESS);
          } else {
            Serial.println(F("Send fail, Test OK"));
          }
        }
      }
    }
    radio.stopListening();
    newdata = false;
  }
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
}
