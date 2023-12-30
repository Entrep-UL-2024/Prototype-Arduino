#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include <ArduinoBLE.h>
#include "spo2_algorithm.h"
#include "heartRate.h"

#define PRINTS

#ifdef PRINTS
#define Print(msg) Serial.print(msg)
#define Println(msg) Serial.println(msg)
#else
#define Print(msg)
#define Println(msg)
#endif

//  Init BLE
const char* ble_name = "ParticleSensor";
unsigned long lastReadTime = 0;
const unsigned long updateInterval = 500; // Update BLE every 1000ms

BLEService particleService("19B10010-E8F2-537E-4F6C-D104768A1214"); // Create a BLE service
BLEUnsignedLongCharacteristic irCharacteristic("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // IR Characteristic
BLEUnsignedLongCharacteristic redCharacteristic("19B10012-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Red Characteristic
BLEUnsignedLongCharacteristic currentCharacteristic("19B10013-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Define BLE Characteristic for Current


float calculateSpO2(uint32_t redValue, uint32_t irValue, float a = -17, float b = 104) {
  // Calculate the ratio of red and IR values
  float ratio = (float)redValue / (float)irValue;

  // Calculate SpO2 using the ratio
  float spo2 = a * ratio + b;

  return spo2;
}

const int RATE_SIZE = 4; // Increase this for more accuracy
int rates[RATE_SIZE];
int rateSpot = 0;
long lastBeat = 0;
bool checkForBeat(uint32_t irValue) {
  // Implement your logic to detect a heartbeat using the IR value
  // Return true if a heartbeat is detected, false otherwise
  // You can use any algorithm or condition that suits your requirements

  // Example logic: Check if the IR value exceeds a certain threshold
  const int IR_THRESHOLD = 1000;
  if (irValue > IR_THRESHOLD) {
    return true;
  } else {
    return false;
  }
}
float calculateHeartRateBPM(uint32_t irValue) {
    float heartRate = 0.0;

    if (checkForBeat(irValue)) { // Assuming checkForBeat is a function that detects a heartbeat
        long delta = millis() - lastBeat;
        lastBeat = millis();

        heartRate = 60 / (delta / 1000.0); // Calculate BPM

        rates[rateSpot++] = (int)heartRate; // Store this reading in the array
        rateSpot %= RATE_SIZE;

        // Average the last few readings
        float beatAvg = 0;
        for (int i = 0; i < RATE_SIZE; i++) {
            beatAvg += rates[i];
        }
        beatAvg /= RATE_SIZE;

        heartRate = beatAvg;
    }

    return heartRate;
}

uint32_t calculateHeartRateUINT(uint32_t irValue){
  // TODO: implement this function
  float heartRate = calculateHeartRateBPM(irValue);
  return (uint32_t)heartRate;
}

void updateBLE(uint32_t irValue, uint32_t redValue, uint32_t currentValue){
  // Update BLE
  if (millis() - lastReadTime > updateInterval) {
    Print(">IRValueBLE:");
    Println(irValue);
    Print(">RedValueBLE:");
    Println(redValue);
    Print(">CurrentBLE:");
    Println(currentValue);

    // calculate heart rate
    float heartRate = calculateHeartRateBPM(irValue);
    Print(">HeartRateBLE:");
    Println(heartRate);
    // calculate SpO2
    float spo2 = calculateSpO2(redValue, irValue);
    Print(">SpO2BLE:");
    Println(spo2);

    irCharacteristic.writeValue(irValue);
    redCharacteristic.writeValue(redValue);
    currentCharacteristic.writeValue(currentValue);
    // update time
    lastReadTime = millis();
  }
}

// Init MAX30102 Oxymeter Sensor
MAX30105 particleSensor; // initialize MAX30102 with I2C
byte ledBrightness = 60; //Options: 0=Off to 255=50mA
byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
byte ledMode = 3; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
int pulseWidth = 411; //Options: 69, 118, 215, 411
int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

// Oxymeter constants
const int BUFFER_SIZE_OX = 10;
uint64_t irSum = 0;
uint32_t irBuffer[BUFFER_SIZE_OX];
int irBufferIndexOx = 0;
int irNumSamplesOx = 0;
uint64_t redSum = 0;
uint32_t redBuffer[BUFFER_SIZE_OX];
int redBufferIndexOx = 0;
int redNumSamplesOx = 0;

void read_data(uint32_t &irValue, uint32_t &redValue){
  // Read IR and Red data
  uint32_t _irValue = particleSensor.getIR();
  uint32_t _redValue = particleSensor.getRed();
  uint32_t _greenValue = particleSensor.getFIFOGreen();
  
  // Update IR circular buffer
  irSum -= irBuffer[irBufferIndexOx];
  irSum += _irValue;
  irBuffer[irBufferIndexOx] = _irValue;
  irBufferIndexOx = (irBufferIndexOx + 1) % BUFFER_SIZE_OX;
  if (irNumSamplesOx < BUFFER_SIZE_OX) irNumSamplesOx++;

  // Update Red circular buffer
  redSum -= redBuffer[redBufferIndexOx];
  redSum += _redValue;
  redBuffer[redBufferIndexOx] = _redValue;
  redBufferIndexOx = (redBufferIndexOx + 1) % BUFFER_SIZE_OX;
  if (redNumSamplesOx < BUFFER_SIZE_OX) redNumSamplesOx++;
  
  // Calculate mean values
  uint32_t meanIR = abs(irSum / irNumSamplesOx);
  uint32_t meanRed = abs(redSum / redNumSamplesOx);

  // Update output values
  irValue = meanIR;
  redValue = meanRed;
  
  // read next set of samples
  particleSensor.nextSample();
};


// Init Current sensor
// Assuming you're using a 5A ACS712. Adjust the sensitivity accordingly for other models.
const float ACS712_SENSITIVITY = 0.185; // Sensitivity in V/A (for 5A module)
const int ACS712_ZERO = 512; // Change this based on your calibration

// Current constants
const int BUFFER_SIZE_CUR = 100;
float sumCurrent = 0.0;
float currentBuffer[BUFFER_SIZE_CUR];
int bufferIndexCur = 0;
int numSamplesCur = 0;

uint32_t readCurrent(){
  int analogValue = analogRead(A0);
  // Convert to voltage (assuming 5V reference and 10-bit ADC)
  float voltage = (analogValue / 1023.0) * 5.0;
  // Convert voltage to current
  float current = (voltage - 2.5) / ACS712_SENSITIVITY; // Offset by 2.5V for ACS712

  // Update circular buffer
  sumCurrent -= currentBuffer[bufferIndexCur];
  sumCurrent += current;
  currentBuffer[bufferIndexCur] = current;
  bufferIndexCur = (bufferIndexCur + 1) % BUFFER_SIZE_CUR;
  if (numSamplesCur < BUFFER_SIZE_CUR) numSamplesCur++;
  
  // Calculate mean current
  float meanCurrent = sumCurrent / numSamplesCur;

  // Convert current to an integer for BLE transmission
  // This will send the current in milliamps
  uint32_t currentForBLE = (uint32_t)abs(meanCurrent * 1000);
  
  return currentForBLE;
}

void setup()
{
  #ifdef PRINTS
  Serial.begin(115200);
  while(!Serial) delay(10); 
  // delay(2500); //wait for serial
  Println("");
  Println("MAX30102 Oxymeter Sensor");
  #endif
  delay(500);

  // Initialize sensor
  if (particleSensor.begin(Wire, I2C_SPEED_FAST) == false) //Use default I2C port, 400kHz speed
  {
    Println("MAX30102 was not found. Please check wiring/power. ");
    while (particleSensor.begin(Wire, I2C_SPEED_FAST) == false);
  }
  Println("MAX30102 found.");

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
  Print("Local BLE Address: ");
  Print(BLE.address());
  Print(" | BLE Peripheral - ");
  Print(ble_name);
  Println(" is now advertising.");
}

void loop() {
  BLEDevice central = BLE.central();
  if (central)
  {
    Print("Connected to central: ");
    Println(central.address());

    while (central.connected())
    {
      particleSensor.check(); //Check the sensor
      while (particleSensor.available()) {
        
        // read data
        uint32_t irValue, redValue;
        read_data(irValue, redValue);
        Print(">IRValue:");
        Println(irValue);
        Print(">RedValue:");
        Println(redValue);

        // calculate heart rate
        float heartRate = calculateHeartRateBPM(irValue);
        Print(">HeartRate:");
        Println(heartRate);
        // calculate SpO2
        float spo2 = calculateSpO2(redValue, irValue);
        Print(">SpO2:");
        Println(spo2);

        // calculate current
        uint32_t currentValue = readCurrent();
        Print(">Current:");
        Println(currentValue);

        // tests prints with a sinus function
        uint32_t sinusValue = (uint32_t) (128 + 127 * sin(micros() / 1000000.0 * 2 * PI * 0.5));
        Print(">Sinus:");
        Println(sinusValue);

        // Update BLE
        updateBLE(irValue, redValue, currentValue);
             
      } 
    }
    Println("Disconnected from central");
  }
}