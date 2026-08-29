#ifndef MAX30102_H
#define MAX30102_H

#include <Arduino.h>

bool MAX30102_begin();
bool MAX30102_hayDedo();
void MAX30102_update();

int MAX30102_getSpO2();
int MAX30102_getHeartRate();

#endif