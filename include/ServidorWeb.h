#ifndef SERVIDOR_WEB_H
#define SERVIDOR_WEB_H

#include <Arduino.h>

void WebServer_begin();

void WebServer_update(
    int bpm,
    int spo2,
    float temperatura
);

// Se llama cada vez que exista una nueva muestra ECG.
void WebServer_agregarMuestraECG(int ecg);

#endif