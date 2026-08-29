#include "HeartRateMAX30102.h"

#include "SensorMAX30102.h"
#include "heartRate.h"

// ================================================================
// CONFIGURACION
// ================================================================

// BPM devuelto al resto del proyecto.
// -1 significa que todavía no existe una lectura válida.
static int bpmEstable = -1;

// Umbral mínimo para considerar que existe un dedo.
// Se deja más flexible que el valor anterior de 50000.
static const uint32_t UMBRAL_DEDO = 10000;

// Cantidad de valores usados para calcular el promedio.
static const byte TOTAL_BPM = 4;

// Tiempo máximo sin detectar un latido válido.
static const unsigned long TIEMPO_SIN_LATIDO_MS = 3000;


// ================================================================
// VARIABLES
// ================================================================

static byte historialBPM[TOTAL_BPM] =
{
    0, 0, 0, 0
};

static byte posicionBPM = 0;
static byte cantidadBPM = 0;

// Permite verificar que no procesemos dos veces la misma muestra.
static unsigned long ultimaMuestraProcesada = 0;

// Tiempo en milisegundos del último pulso detectado.
static unsigned long tiempoUltimoLatido = 0;


// ================================================================
// REINICIAR CALCULO
// ================================================================

static void reiniciarBPM()
{
    bpmEstable = -1;

    posicionBPM = 0;
    cantidadBPM = 0;

    tiempoUltimoLatido = 0;

    for (byte i = 0; i < TOTAL_BPM; i++)
    {
        historialBPM[i] = 0;
    }
}


// ================================================================
// INICIALIZACION
// ================================================================

void HeartRate_begin()
{
    ultimaMuestraProcesada =
        SensorMAX30102_getNumeroMuestra();

    reiniciarBPM();
}


// ================================================================
// ACTUALIZACION
// ================================================================

void HeartRate_update()
{
    unsigned long numeroMuestra =
        SensorMAX30102_getNumeroMuestra();

    // No procesar dos veces la misma muestra.
    if (numeroMuestra == ultimaMuestraProcesada)
    {
        return;
    }

    ultimaMuestraProcesada = numeroMuestra;

    uint32_t valorIR =
        SensorMAX30102_getIR();

    unsigned long tiempoActual =
        millis();

    // ============================================================
    // COMPROBAR SI EXISTE UN DEDO
    // ============================================================

    if (valorIR < UMBRAL_DEDO)
    {
        reiniciarBPM();
        return;
    }

    // ============================================================
    // DETECTAR UN PULSO
    // ============================================================

    if (checkForBeat(valorIR))
    {
        // El primer pulso solo sirve como referencia temporal.
        if (tiempoUltimoLatido == 0)
        {
            tiempoUltimoLatido = tiempoActual;
            return;
        }

        unsigned long intervalo =
            tiempoActual - tiempoUltimoLatido;

        tiempoUltimoLatido = tiempoActual;

        if (intervalo == 0)
        {
            return;
        }

        // BPM calculado mediante el tiempo real entre pulsos.
        float bpmInstantaneo =
            60000.0f / intervalo;

        // Rechazar resultados fuera de un rango razonable.
        if (bpmInstantaneo < 40.0f ||
            bpmInstantaneo > 200.0f)
        {
            return;
        }

        historialBPM[posicionBPM] =
            static_cast<byte>(bpmInstantaneo);

        posicionBPM =
            (posicionBPM + 1) % TOTAL_BPM;

        if (cantidadBPM < TOTAL_BPM)
        {
            cantidadBPM++;
        }

        int suma = 0;

        for (byte i = 0; i < cantidadBPM; i++)
        {
            suma += historialBPM[i];
        }

        bpmEstable =
            suma / cantidadBPM;
    }

    // ============================================================
    // REINICIAR SI DEJA DE DETECTAR PULSOS
    // ============================================================

    if (tiempoUltimoLatido > 0 &&
        tiempoActual - tiempoUltimoLatido >
            TIEMPO_SIN_LATIDO_MS)
    {
        reiniciarBPM();
    }
}


// ================================================================
// OBTENER BPM
// ================================================================

int HeartRate_getBPM()
{
    return bpmEstable;
}