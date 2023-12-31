#ifndef OX_SENSOR_H
#define OX_SENSOR_H

#include <Arduino.h>

void initializeSensor();

void update_mean_HR_Spo2(int32_t _hr, int8_t _hrValid, int32_t _spo2, int8_t _spo2Valid);

void calibrate();

void read_ox();

int32_t get_spo2();

int32_t get_hr();

#endif // OX_SENSOR_H