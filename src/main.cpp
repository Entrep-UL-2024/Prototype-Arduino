#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include <ArduinoBLE.h>

// Assuming you're using a 5A ACS712. Adjust the sensitivity accordingly for other models.
const float ACS712_SENSITIVITY = 0.185; // Sensitivity in V/A (for 5A module)
const int ACS712_ZERO = 512; // Change this based on your calibration

const char* ble_name = "ParticleSensor";

BLEService particleService("19B10010-E8F2-537E-4F6C-D104768A1214"); // Create a BLE service

BLEUnsignedCharCharacteristic irCharacteristic("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // IR Characteristic
BLEUnsignedCharCharacteristic redCharacteristic("19B10012-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Red Characteristic

// Define BLE Characteristic for Current
BLEUnsignedIntCharacteristic currentCharacteristic("19B10013-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);

MAX30105 particleSensor; // initialize MAX30102 with I2C

float sumCurrent = 0.0;
int numReadings = 0;
const int BUFFER_SIZE = 100;
float currentBuffer[BUFFER_SIZE];
int bufferIndex = 0;

uint8_t readCurrent(){
  int analogValue = analogRead(A0);
  // Convert to voltage (assuming 5V reference and 10-bit ADC)
  float voltage = (analogValue / 1023.0) * 5.0;
  // Convert voltage to current
  float current = (voltage - 2.5) / ACS712_SENSITIVITY; // Offset by 2.5V for ACS712

  // Update circular buffer
  sumCurrent -= currentBuffer[bufferIndex];
  sumCurrent += current;
  currentBuffer[bufferIndex] = current;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
  
  // Calculate mean current
  float meanCurrent = sumCurrent / BUFFER_SIZE;

  // Convert current to an integer for BLE transmission
  // This will send the current in milliamps
  uint8_t currentForBLE = abs(meanCurrent * 1000);
  
  return currentForBLE;
}

void setup()
{
  Serial.begin(115200);
  // while(!Serial) delay(50); //We must wait for Teensy to come online
  delay(2500); //wait for serial
  Serial.println("");
  Serial.println("MAX30102");
  delay(500);

  // Initialize sensor
  if (particleSensor.begin(Wire, I2C_SPEED_FAST) == false) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (particleSensor.begin(Wire, I2C_SPEED_FAST) == false);
  }
  Serial.println("MAX30105 found.");

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  // BLE setup
  BLE.begin();
  BLE.setLocalName(ble_name);
  BLE.setAdvertisedService(particleService);
  particleService.addCharacteristic(irCharacteristic);
  particleService.addCharacteristic(redCharacteristic);
  particleService.addCharacteristic(currentCharacteristic);
  BLE.addService(particleService);
  BLE.advertise();

  // Print the local BLE address
  Serial.print("Local BLE Address: ");
  Serial.print(BLE.address());
  Serial.print(" | BLE Peripheral - ");
  Serial.print(ble_name);
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
      particleSensor.check(); //Check the sensor
      while (particleSensor.available()) {
        // Read IR and Red data
        uint8_t irValue = particleSensor.getFIFOIR();
        uint8_t redValue = particleSensor.getFIFORed();
        uint8_t greenValue = particleSensor.getFIFOGreen();
        particleSensor.getRed();
        
        uint8_t currentValue = readCurrent();


         // read microseconds
        // Serial.print("micros ");
        // Serial.println(micros());
        // read stored IR
        Serial.print(">IRValue:");
        Serial.println(irValue);
        // read stored red
        Serial.print(">RedValue:");
        Serial.println(redValue);
        // read current
        Serial.print(">Current:");
        Serial.println(currentValue);

        uint8_t sinusValue = (uint8_t) (128 + 127 * sin(micros() / 1000000.0 * 2 * PI * 0.5));
        Serial.print(">Sinus:");
        Serial.println(sinusValue);

        // Update BLE characteristics
        // irCharacteristic.writeValue(sinusValue);
        // redCharacteristic.writeValue(redValue);
        // currentCharacteristic.writeValue(currentValue);

        delay(10); // Add a small delay to avoid overloading BLE

        // read next set of samples
        particleSensor.nextSample();      
      } 
    }
    Serial.println("Disconnected from central");
  }
}