#include "current.h"
#include "utility.h"

// Init Current sensor constants
// Assuming you're using a 5A ACS712. Adjust the sensitivity accordingly for other models.
const float ACS712_SENSITIVITY = 0.185; // Sensitivity in V/A (for 5A module)
const int ACS712_ZERO = 512; // Change this based on your calibration
const int BUFFER_SIZE_CUR = 100;
const uint8_t PIN_CURRENT_SENSOR = A0;

float sumCurrent = 0.0;
float currentBuffer[BUFFER_SIZE_CUR];
int bufferIndexCur = 0;
int numSamplesCur = 0;
uint32_t readCurrent(){
  int analogValue = analogRead(PIN_CURRENT_SENSOR);
  // Convert to voltage (assuming 5V reference and 10-bit ADC)
  float voltage = (analogValue / 1023.0) * 5.0;
  // Convert voltage to current
  float current = (voltage - 2.5) / ACS712_SENSITIVITY; // Offset by 2.5V for ACS712

  Print(">Current: ");
  Println(current);

  // Update circular buffer
  sumCurrent -= currentBuffer[bufferIndexCur];
  sumCurrent += current;
  currentBuffer[bufferIndexCur] = current;
  bufferIndexCur = (bufferIndexCur + 1) % BUFFER_SIZE_CUR;
  if (numSamplesCur < BUFFER_SIZE_CUR) numSamplesCur++;
  
  // Calculate mean current
  float meanCurrent = sumCurrent / numSamplesCur;

  Print(">Mean Current: ");
  Println(meanCurrent);

  // Convert current to an integer for BLE transmission
  // This will send the current in milliamps
  uint32_t currentForBLE = (uint32_t)abs(meanCurrent * 1000);

  return currentForBLE;
}