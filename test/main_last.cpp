#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include <ArduinoBLE.h>
#include "spo2_algorithm.h"

#define PRINTS
// #define USE_BLE

#ifdef PRINTS
#define Print(msg) Serial.print(msg)
#define Println(msg) Serial.println(msg)
#else
#define Print(msg)
#define Println(msg)
#endif

//  BLE constants
const char* ble_name = "ParticleSensor";
const unsigned long updateInterval = 500; // Update BLE every 1000ms
// Init MAX30102 Oxymeter Sensor constants
MAX30105 particleSensor; // initialize MAX30102 with I2C
byte ledBrightness = 60; //Options: 0=Off to 255=50mA
byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
byte ledMode = 3; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
int pulseWidth = 411; //Options: 69, 118, 215, 411
int adcRange = 4096; //Options: 2048, 4096, 8192, 16384
// Oxymeter constants
const int BUFFER_SIZE_OX = 100;
const int BATCH_SIZE = 5;
const int BUFFER_SIZE_HR = 10;
const int BUFFER_SIZE_SPO2 = 10;
// Init Current sensor constants
// Assuming you're using a 5A ACS712. Adjust the sensitivity accordingly for other models.
const float ACS712_SENSITIVITY = 0.185; // Sensitivity in V/A (for 5A module)
const int ACS712_ZERO = 512; // Change this based on your calibration
const int BUFFER_SIZE_CUR = 100;

// Init BLE
BLEService particleService("19B10010-E8F2-537E-4F6C-D104768A1214"); // Create a BLE service
BLEUnsignedLongCharacteristic hRCharacteristic("19B10011-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // IR Characteristic
BLEUnsignedLongCharacteristic spo2Characteristic("19B10012-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Red Characteristic
BLEUnsignedLongCharacteristic currentCharacteristic("19B10013-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify); // Define BLE Characteristic for Current

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

    hRCharacteristic.writeValue(hrValue);
    spo2Characteristic.writeValue(spo2Value);
    currentCharacteristic.writeValue(currentValue);
    // update time
    lastReadTime = millis();
  }
}

int32_t hrBuffer[BUFFER_SIZE_HR];
int32_t spo2Buffer[BUFFER_SIZE_SPO2];
int64_t hrSum = 0;
int hrBufferIndex = 0;
int hrNumSamples = 0;
int64_t spo2Sum = 0;
int spo2BufferIndex = 0;
int spo2NumSamples = 0;

int32_t hrMean = 0;
int32_t spo2Mean = 0;
void update_mean_HR_Spo2(int32_t _hr, int8_t _hrValid, int32_t _spo2, int8_t _spo2Valid){
  if (_hrValid != 0){
    hrSum -= hrBuffer[hrBufferIndex];
    hrSum += _hr;
    hrBuffer[hrBufferIndex] = _hr;
    hrBufferIndex = (hrBufferIndex + 1) % BUFFER_SIZE_HR;
    if (hrNumSamples < BUFFER_SIZE_HR) hrNumSamples++;
    // Calculate mean values
    int32_t _meanHR = abs(hrSum / hrNumSamples);
    // Update output values
    hrMean = _meanHR;
  }
  if (_spo2Valid != 0){
    spo2Sum -= spo2Buffer[spo2BufferIndex];
    spo2Sum += _spo2;
    spo2Buffer[spo2BufferIndex] = _spo2;
    spo2BufferIndex = (spo2BufferIndex + 1) % BUFFER_SIZE_SPO2;
    if (spo2NumSamples < BUFFER_SIZE_SPO2) spo2NumSamples++;
    // Calculate mean values
    int32_t _meanSpo2 = abs(spo2Sum / spo2NumSamples);
    // Update output values
    spo2Mean = _meanSpo2;
  }
};

uint32_t irBuffer[BUFFER_SIZE_OX];
uint32_t redBuffer[BUFFER_SIZE_OX];

int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

void calibrate(){
  int bufferLength = BUFFER_SIZE_OX; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample

    Print("red=");
    Print(redBuffer[i]);
    Print(", ir=");
    Println(irBuffer[i]);
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  update_mean_HR_Spo2(heartRate, validHeartRate, spo2, validSPO2);
}

void get_data(int batch_size = BATCH_SIZE){
  
    int bufferLength = BUFFER_SIZE_OX; //buffer length of 100 stores 4 seconds of samples running at 25sps
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = batch_size; i < bufferLength; i++)
    {
      redBuffer[i - batch_size] = redBuffer[i];
      irBuffer[i - batch_size] = irBuffer[i];
    }

    //take 25 sets of samples before calculating the heart rate.
    for (byte i = (bufferLength - batch_size); i < bufferLength; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample

      //send samples and calculation result to terminal program through UART
      // Print(">red:");
      // Println(redBuffer[i]);
      // Print(">ir:");
      // Println(irBuffer[i]);

      Print(">HR:");
      Println(heartRate);

      Print(">HRvalid:");
      Println(validHeartRate);

      Print(">SPO2:");
      Println(spo2);

      Print(">SPO2Valid:");
      Println(validSPO2);

      // update mean
      update_mean_HR_Spo2(heartRate, validHeartRate, spo2, validSPO2);

      Print(">HRMean:");
      Println(hrMean);

      Print(">SPO2Mean:");
      Println(spo2Mean);

    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
}

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
  delay(2500); //wait for serial
  #endif
  Println("");
  Println("MAX30102 Oxymeter Sensor");

  // Initialize sensor
  if (particleSensor.begin(Wire, I2C_SPEED_FAST) == false) //Use default I2C port, 400kHz speed
  {
    Println("MAX30102 was not found. Please check wiring/power. ");
    while (particleSensor.begin(Wire, I2C_SPEED_FAST) == false);
  }
  Println("MAX30102 found.");

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  #ifdef USE_BLE
  // BLE setup
  BLE.begin();
  BLE.setLocalName(ble_name);
  BLE.setAdvertisedService(particleService);
  particleService.addCharacteristic(hRCharacteristic);
  particleService.addCharacteristic(spo2Characteristic);
  particleService.addCharacteristic(currentCharacteristic);
  BLE.addService(particleService);
  BLE.advertise();

  // Print the local BLE address
  Print("Local BLE Address: ");
  Print(BLE.address());
  Print(" | BLE Peripheral - ");
  Print(ble_name);
  Println(" is now advertising.");
  #endif
}

void loop() {
  #ifdef USE_BLE
  BLEDevice central = BLE.central();
  bool isCentralConnected = central && central.connected();
  #else
  bool isCentralConnected = true;
  #endif
  if (isCentralConnected)
  {
    #ifdef USE_BLE
    Print("Connected to central: ");
    Println(central.address());
    #endif
    while (isCentralConnected)
    {
      calibrate();
      Println("Calibration done");
      while (isCentralConnected){

        get_data();

        uint32_t currentValue = readCurrent();

        // Update BLE
        #ifdef USE_BLE
        updateBLE(hrMean, spo2Mean, currentValue);
        #endif
      } 
    }
    #ifdef USE_BLE
    Println("Disconnected from central");
    #endif
  }
}