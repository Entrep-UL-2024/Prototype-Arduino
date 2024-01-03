#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include <ArduinoBLE.h>

// Assuming you're using a 5A ACS712. Adjust the sensitivity accordingly for other models.
const float ACS712_SENSITIVITY = 0.185; // Sensitivity in V/A (for 5A module)
const int ACS712_ZERO = 512; // Change this based on your calibration

const char* _ble_name = "ParticleSensor";

BLEService _particleService("19B10010-E8F2-537E-4F6C-D104768A1214"); // Create a BLE service

BLEUnsignedCharCharacteristic _irCharacteristic("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // IR Characteristic
BLEUnsignedCharCharacteristic _redCharacteristic("19B10012-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Red Characteristic

// Define BLE Characteristic for Current
BLEUnsignedIntCharacteristic _currentCharacteristic("19B10013-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);

MAX30105 _particleSensor; // initialize MAX30102 with I2C

uint8_t _readCurrent(){
  int analogValue = analogRead(A0);
  // Convert to voltage (assuming 5V reference and 10-bit ADC)
  float voltage = (analogValue / 1023.0) * 5.0;
  // Convert voltage to current
  float current = (voltage - 2.5) / ACS712_SENSITIVITY; // Offset by 2.5V for ACS712
  // Convert current to an integer for BLE transmission
  // This will send the current in milliamps
  uint8_t currentForBLE = abs(current * 1000);
  return currentForBLE;
}

void setup()
{
  Serial.begin(115200);
  // while(!Serial) delay(50); //We must wait for Teensy to come online
  delay(1000);

  delay(500);
  Serial.println("");
  Serial.println("MAX30102");
  delay(500);

  // Initialize sensor
  if (_particleSensor.begin(Wire, I2C_SPEED_FAST) == false) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (_particleSensor.begin(Wire, I2C_SPEED_FAST) == false);
  }
  Serial.println("MAX30105 found.");

  byte ledBrightness = 70; //Options: 0=Off to 255=50mA
  byte sampleAverage = 1; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  int sampleRate = 400; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 69; //Options: 69, 118, 215, 411
  int adcRange = 16384; //Options: 2048, 4096, 8192, 16384

  _particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  // BLE setup
  BLE.begin();
  BLE.setLocalName(_ble_name);
  BLE.setAdvertisedService(_particleService);
  _particleService.addCharacteristic(_irCharacteristic);
  _particleService.addCharacteristic(_redCharacteristic);
  _particleService.addCharacteristic(_currentCharacteristic);
  BLE.addService(_particleService);
  BLE.advertise();

  // Print the local BLE address
  Serial.print("Local BLE Address: ");
  Serial.print(BLE.address());
  Serial.print(" | BLE Peripheral - ");
  Serial.print(_ble_name);
  Serial.println(" is now advertising.");
}

void loop() {
  BLEDevice central = BLE.central();
  if (central)
  {
    Serial.print("Connected to central: ");
    Serial.println(central.address());

    while (central.connected())
    {
      _particleSensor.check(); //Check the sensor
      while (_particleSensor.available()) {
        // Read IR and Red data
        uint8_t irValue = _particleSensor.getFIFOIR();
        uint8_t redValue = _particleSensor.getFIFORed();
        uint8_t currentValue = _readCurrent();

        // read microseconds
        // Serial.print("micros ");
        // Serial.println(micros());
        // // read stored IR
        // Serial.print(">IR:");
        // Serial.print(irValue);
        // // read stored red
        // Serial.print(">Red:");
        // Serial.println(redValue);

        // Serial.print(">Current:");
        // Serial.println(currentValue);

        // Update BLE characteristics
        _irCharacteristic.writeValue(irValue);
        _redCharacteristic.writeValue(redValue);
        _currentCharacteristic.writeValue(currentValue);

        delay(10); // Add a small delay to avoid overloading BLE

        // read next set of samples
        _particleSensor.nextSample();      
      } 
    }
    Serial.println("Disconnected from central");
  }
}