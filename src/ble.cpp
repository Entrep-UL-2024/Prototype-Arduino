#include "ble.h"
#include "utility.h"
#include <ArduinoBLE.h>

//  BLE constants
const char* ble_name = "ParticleSensor";
const unsigned long updateInterval = 500; // Update BLE every 1000ms

// Init BLE
BLEService particleService("19B10010-E8F2-537E-4F6C-D104768A1214"); // Create a BLE service
BLEUnsignedLongCharacteristic hRCharacteristic("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // IR Characteristic
BLEUnsignedLongCharacteristic spo2Characteristic("19B10012-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Red Characteristic
BLEUnsignedLongCharacteristic currentCharacteristic("19B10013-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Define BLE Characteristic for Current

void initializeBLE()
{
  #ifdef USE_BLE
  // BLE setup
  BLE.begin();
  BLE.setLocalName(ble_name);
  BLE.setAdvertisedService(particleService);
  particleService.addCharacteristic(hRCharacteristic);
  particleService.addCharacteristic(spo2Characteristic);
  particleService.addCharacteristic(currentCharacteristic);
  BLE.addService(particleService);
  advertiseBLE();
  #endif
}

BLEDevice central;
bool connectBle(){
    #ifdef USE_BLE
    central = BLE.central();
    bool isCentralConnected = central && central.connected();
    #else
    bool isCentralConnected = true;
    #endif
    return isCentralConnected;
}

bool isBleConnected(){
  #ifdef USE_BLE
  return central && central.connected();
  #else
  return true;
  #endif
}

void advertiseBLE()
{
  #ifdef USE_BLE
  BLE.advertise();
  // Print the local BLE address
  Print("Local BLE Address: ");
  Print(BLE.address());
  Print(" | BLE Peripheral - ");
  Print(ble_name);
  Println(" is now advertising.");
  #endif
}

String getBleAddress()
{
  if (central && central.connected())
    return central.address();
  else
    return "Not connected";
}

unsigned long lastReadTime = 0;
void updateBLE(uint32_t hrValue, uint32_t spo2Value, uint32_t currentValue){
  // Update BLE
  if (millis() - lastReadTime > updateInterval) {
    Print(">HRValueBLE:");
    Println(hrValue);
    Print(">Spo2ValueBLE:");
    Println(spo2Value);
    Print(">CurrentBLE:");
    Println(currentValue);

    #ifdef USE_BLE
    hRCharacteristic.writeValue(hrValue);
    spo2Characteristic.writeValue(spo2Value);
    currentCharacteristic.writeValue(currentValue);
    #endif

    // update time
    lastReadTime = millis();
  }
}