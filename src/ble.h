#ifndef _BLE_H
#define _BLE_H

#include <Arduino.h>

// Init BLE
void initializeBLE();

bool connectBle();

bool isBleConnected();

void advertiseBLE();

String getBleAddress();

void updateBLE(uint32_t hrValue, uint32_t spo2Value, uint32_t currentValue);

#endif // _BLE_H