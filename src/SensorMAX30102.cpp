#include "SensorMAX30102.h"

MAX30105 sensor;

static uint32_t ultimoIR = 0;
static uint32_t ultimoRed = 0;
static unsigned long numeroMuestra = 0;

bool SensorMAX30102_begin()
{
    if (!sensor.begin(Wire, I2C_SPEED_FAST))
        return false;

    byte brillo = 60;
    byte promedio = 4;
    byte modoLED = 2;
    int frecuencia = 100;
    int anchoPulso = 411;
    int adc = 4096;

    sensor.setup(
        brillo,
        promedio,
        modoLED,
        frecuencia,
        anchoPulso,
        adc
    );

    return true;
}

bool SensorMAX30102_update()
{
    sensor.check();

    if (!sensor.available())
        return false;

    ultimoRed = sensor.getRed();
    ultimoIR = sensor.getIR();
    numeroMuestra++;

    sensor.nextSample();

    return true;
}

uint32_t SensorMAX30102_getIR()
{
    return ultimoIR;
}

uint32_t SensorMAX30102_getRed()
{
    return ultimoRed;
}

unsigned long SensorMAX30102_getNumeroMuestra()
{
    return numeroMuestra;
}

void SensorMAX30102_apagar()
{
    sensor.shutDown();
}

void SensorMAX30102_encender()
{
    sensor.wakeUp();
}