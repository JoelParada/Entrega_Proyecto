#ifndef SESIONES_H
#define SESIONES_H

#include <Arduino.h>

bool Sesion_iniciar(const String& cedula);
void Sesion_actualizar(
    int bpm,
    int spo2,
    int ecg
);
bool Sesion_finalizar();

bool Sesion_activa();
String Sesion_rutaActual();

#endif