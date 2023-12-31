#ifndef UTILITY_H
#define UTILITY_H

#include <Arduino.h>

#define PRINTS
// #define USE_BLE

#ifdef PRINTS
#define Print(msg) Serial.print(msg)
#define Println(msg) Serial.println(msg)
#else
#define Print(msg)
#define Println(msg)
#endif

void initializePrint();

#endif // UTILITY_H