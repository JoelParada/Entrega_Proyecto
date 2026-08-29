#ifndef SENSOR_MAX30102_H
#define SENSOR_MAX30102_H

#include <Arduino.h>
#include "MAX30105.h"

extern MAX30105 sensor;

// Inicializa el sensor
bool SensorMAX30102_begin();

// Lee una muestra del MAX30102.
// Devuelve true únicamente cuando existe una muestra nueva.
bool SensorMAX30102_update();

// Última muestra disponible
uint32_t SensorMAX30102_getIR();
uint32_t SensorMAX30102_getRed();

unsigned long SensorMAX30102_getNumeroMuestra();

void SensorMAX30102_apagar();
void SensorMAX30102_encender();

#endif