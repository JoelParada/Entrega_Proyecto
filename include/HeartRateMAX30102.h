#ifndef HEARTRATE_MAX30102_H
#define HEARTRATE_MAX30102_H

#include <Arduino.h>

void HeartRate_begin();

void HeartRate_update();

int HeartRate_getBPM();

#endif