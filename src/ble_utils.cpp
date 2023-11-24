#include <NimBLEDevice.h>
#include "ble_utils.h"
#include "msg_utils.h"
#include "lora_utils.h"
#include "display.h"
#include "logger.h"
#include "KISS_TO_TNC2.h"
#include "ax25_utils.h"

#include <iostream>
#include <string>

#define SERVICE_UUID            "00000001-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_TX  "00000003-ba2a-46c9-ae49-01b0961f68bb"
#define CHARACTERISTIC_UUID_RX  "00000002-ba2a-46c9-ae49-01b0961f68bb"

BLEServer *pServer;
BLECharacteristic *pCharacteristicTx;
BLECharacteristic *pCharacteristicRx;

extern logging::Logger  logger;
extern bool             sendBleToLoRa;
extern String           bleLoRaPacket;
extern int              bleMsgCompose;
extern String           bleMsgAddresse;
extern String           bleMsgTxt;
extern bool             bleConnected;
extern String           BLEToLoRaPacket;


class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      bleConnected = true;
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE Client Connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
      bleConnected = false;
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "BLE client Disconnected, Started Advertising");
      pServer->startAdvertising();
    }
};

class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string receivedData = pCharacteristic->getValue();       // Read the data from the characteristic
    
    /*for (int i=0; i<receivedData.size(); i++) {
      Serial.print(receivedData[i],HEX);
    }*/

    String receivedString = "";
    for (int i=0; i<receivedData.length()-2;i++) {
      receivedString += receivedData[i];
    }

    int strLength = receivedString.length();
    byte *byteArray = (byte *)malloc(strLength);

    if (byteArray != NULL) {
      receivedString.getBytes(byteArray, strLength);  // Use the getBytes() function to fill the byte array from the string

      // Print the contents of the byte array
      /*for (int i = 0; i < strLength; i++) {
        Serial.print(myByteArray[i]);
        Serial.print(" ");
      }*/

      // Free the dynamically allocated memory
      
    } else {
      Serial.println("Memory allocation failed!");
    }




    /*const char* charData = receivedData.c_str();
    size_t size = receivedData.size();
    unsigned char* byteArray = new unsigned char[size];
    for (size_t i=0;i<size;++i) {
      byteArray[i] = static_cast<unsigned char>(charData[i]);
    }*/

    BLEToLoRaPacket = AX25_Utils::processAX25(byteArray);
    //
    Serial.println(BLEToLoRaPacket); //just validation
    //
    sendBleToLoRa = true;
    free(byteArray);
    //delete[] byteArray;
  }
};


namespace BLE_Utils {

  void setup() {
    BLEDevice::init("LoRa APRS Tracker");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);


    pCharacteristicTx = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_TX,
                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                        );
    pCharacteristicRx = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_RX,
                          NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
                        );

    pCharacteristicRx->setCallbacks(new MyCallbacks());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pServer->getAdvertising()->setScanResponse(true);
    pServer->getAdvertising()->setMinPreferred(0x06);
    pServer->getAdvertising()->setMaxPreferred(0x0C);

    pAdvertising->start(); //    pServer->getAdvertising()->start();

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE", "%s", "Waiting for BLE central to connect...");
  }

  void sendToLoRa() {
    if (!sendBleToLoRa) {
      return;
    }

    logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE Tx", "%s", BLEToLoRaPacket.c_str());
    show_display("BLE Tx >>", "", BLEToLoRaPacket, 1000);

    LoRa_Utils::sendNewPacket(BLEToLoRaPacket);
    BLEToLoRaPacket = "";
    sendBleToLoRa = false;
  }

  void sendToPhone(const String& packet) {
    if (!packet.isEmpty()) {
      logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "BLE Rx", "%s", packet.c_str());
      String receivedPacketString = "";
      for (int i=0; i<packet.length();i++) {
        receivedPacketString += packet[i];
      }
      //pCharacteristicTx->setValue(receivedPacketString.c_str());
      //pCharacteristicTx->setValue((const uint8_t*)receivedPacketStrin, strlen(receivedPacketStrin));
      //pCharacteristicTx->setValue((uint8_t *)receivedPacketString.c_str(), 20);
      //pCharacteristicTx->setValue((uint8_t *)receivedPacketString.c_str(), receivedPacketString.length());
      
      /*int parts = (receivedPacketString.length()/20) + 1;
      for(int n=0;n<parts;n++) {   
        pCharacteristicTx->setValue(receivedPacketString.substring(n*20, 20)); 
        pCharacteristicTx->notify();
        delay(10);                                                                                // Bluetooth stack will go into congestion, if too many packets are sent
      }*/

      pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fend);
      pCharacteristicTx->notify();
      delay(3);
      pCharacteristicTx->setValue((byte)KissCommandCode::Data);
      pCharacteristicTx->notify();
      delay(3);


      for(int n=0;n<receivedPacketString.length();n++) {   
        uint8_t _c = receivedPacketString[n];
        if (_c == KissSpecialCharacter::Fend) {
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fesc);
          pCharacteristicTx->notify();
          delay(3);
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Tfend);
          pCharacteristicTx->notify();
          delay(3);
        } else if (_c == KissSpecialCharacter::Fesc) {
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fesc);
          pCharacteristicTx->notify();
          delay(3);
          pCharacteristicTx->setValue((byte)KissSpecialCharacter::Tfesc);
          pCharacteristicTx->notify();
          delay(3);
        } else {
          pCharacteristicTx->setValue(&_c, 1);
          pCharacteristicTx->notify();
          delay(3);
        }       
      }
      pCharacteristicTx->setValue((byte)KissSpecialCharacter::Fend);
      pCharacteristicTx->notify();
    }
  }

}
