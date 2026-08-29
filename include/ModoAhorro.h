/////////////////}
#ifndef MODO_AHORRO_H
#define MODO_AHORRO_H

#include <Arduino.h>

void ModoAhorro_begin();

void ModoAhorro_update();

void ModoAhorro_cambiar();

void ModoAhorro_registrarActividad();

bool ModoAhorro_estaActivo();

#endif