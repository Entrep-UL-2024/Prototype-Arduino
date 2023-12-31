#include "oxSensor.h"
#include "Wire.h"
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "utility.h"

const int BUFFER_SIZE_OX = 100;
const int BATCH_SIZE = 5;
const int BUFFER_SIZE_HR = 10;
const int BUFFER_SIZE_SPO2 = 10;

const byte ledBrightness = 60;
const byte sampleAverage = 4;
const byte ledMode = 3;
const byte sampleRate = 100;
const int pulseWidth = 411;
const int adcRange = 4096;

MAX30105 particleSensor;

void initializeSensor() {
    // Initialize sensor
  if (particleSensor.begin(Wire, I2C_SPEED_FAST) == false) //Use default I2C port, 400kHz speed
  {
    Println("MAX30102 was not found. Please check wiring/power. ");
    while (particleSensor.begin(Wire, I2C_SPEED_FAST) == false);
  }
  Println("MAX30102 found.");

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

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

void read_ox(){
  
    int bufferLength = BUFFER_SIZE_OX; //buffer length of 100 stores 4 seconds of samples running at 25sps
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = BATCH_SIZE; i < bufferLength; i++)
    {
      redBuffer[i - BATCH_SIZE] = redBuffer[i];
      irBuffer[i - BATCH_SIZE] = irBuffer[i];
    }

    //take 25 sets of samples before calculating the heart rate.
    for (byte i = (bufferLength - BATCH_SIZE); i < bufferLength; i++)
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

int32_t get_spo2()
{
    return spo2Mean;
}

int32_t get_hr()
{
    return hrMean;
}