#ifndef ECG_H
#define ECG_H


#include <Arduino.h>

//=========================
// CONFIGURACIÓN
//=========================
void ECG_begin();
void ECG_update();

//=========================
// VARIABLES DE SALIDA
//=========================
int ECG_getBPM();
int ECG_getValue();
bool ECG_getBeat();

// Devuelve true cuando algún electrodo está desconectado.
bool ECG_electrodosDesconectados();

#endif