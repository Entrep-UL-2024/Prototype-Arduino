#include "utility.h"
#include "oxSensor.h"
#include "current.h"
#include "ble.h"

void setup()
{
  initializePrint();
  initializeSensor();
  initializeBLE();
}

void loop() {
  if (connectBle())
  {
    #ifdef USE_BLE
    Print("Connected to central: ");
    Println(getBleAddress());
    #endif
    while (isBleConnected())
    {
      calibrate();
      Println("Calibration done");

      while (isBleConnected()){

        read_ox();
        uint32_t hrMean = get_hr();
        uint32_t spo2Mean = get_spo2();

        uint32_t currentValue = readCurrent();

        updateBLE(hrMean, spo2Mean, currentValue);
      } 
    }
    #ifdef USE_BLE
    Println("Disconnected from central");
    #endif
  }
  else
  {
    Println("No central connected - advertising BLE");
    advertiseBLE();
    delay(2500);
  }
}