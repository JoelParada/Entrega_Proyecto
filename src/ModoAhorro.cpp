#include "ModoAhorro.h"

#include "SensorMAX30102.h"
#include "Sesiones.h"

bool modoAhorro = false;

unsigned long ultimaActividad = 0;
unsigned long ultimoCambioModo = 0;

//ANTIRREBOTE
const unsigned long TIEMPO_ANTIRREBOTE = 400;

// TIEMPO DE ESPERA PARA SUSPENSIÓN
const unsigned long TIEMPO_AUTO_AHORRO = 60000;

void ModoAhorro_begin()
{
    ultimaActividad = millis();
}

void ModoAhorro_registrarActividad()
{
    ultimaActividad = millis();

    if (modoAhorro)
    {
        ModoAhorro_cambiar();
    }
}

bool ModoAhorro_estaActivo()
{
    return modoAhorro;
}

void ModoAhorro_update()
{
    if (Sesion_activa())
        return;

    if (!modoAhorro &&
        millis() - ultimaActividad >= TIEMPO_AUTO_AHORRO)
    {
        ModoAhorro_cambiar();
    }
}

void ModoAhorro_cambiar()
{
    modoAhorro = !modoAhorro;

    if (modoAhorro)
    {
        SensorMAX30102_apagar();

        Serial.println("===== MODO AHORRO ACTIVADO =====");
    }
    else
    {
        SensorMAX30102_encender();

        ultimaActividad = millis();

        Serial.println("===== MODO ACTIVO =====");
    }
}