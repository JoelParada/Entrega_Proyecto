#include "MAX30102.h"
#include "SensorMAX30102.h"

#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"

// ================================================================
// BUFERES PARA SpO2
// ================================================================

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2 = 0;
int8_t spo2Valido = 0;

// El algoritmo Maxim también calcula HR, pero no lo utilizaremos
// porque actualmente devuelve valores incorrectos como 250 BPM.
int32_t heartRateMaxim = 0;
int8_t hrMaximValido = 0;

// ================================================================
// VARIABLES DE SpO2
// ================================================================

int spo2Estable = -1;
bool dedoDetectado = false;

const uint32_t UMBRAL_DEDO = 50000;

unsigned long ultimoDedoDetectado = 0;
//unsigned long ultimaMuestra = 0;

int indiceMuestra = 0;

// ================================================================
// VARIABLES PARA BPM
// ================================================================

int bpmEstable = -1;

// ================================================================
// FILTRO ROBUSTO PARA BPM MAXIM
// ================================================================

const byte TOTAL_RESULTADOS_BPM = 9;

int historialMaxim[TOTAL_RESULTADOS_BPM] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

byte indiceMaxim = 0;
byte cantidadMaxim = 0;

// Control de actualización.
unsigned long ultimaActualizacionBPM = 0;

// Fase inicial de estabilización.
int historialInicial[3] =
{
    0, 0, 0
};

byte cantidadInicial = 0;

// Confirmación de cambios grandes.
int bpmCandidato = -1;
byte repeticionesCandidato = 0;

// Ajustes generales.
const int BPM_MINIMO_VALIDO = 30;
const int BPM_MAXIMO_VALIDO = 220;

// Diferencia máxima entre resultados iniciales.
const int TOLERANCIA_INICIAL = 12;

// Cambio normal que puede seguirse suavemente.
const int CAMBIO_NORMAL_MAXIMO = 12;

// Diferencia permitida entre candidatos.
const int TOLERANCIA_CANDIDATO = 10;

// Cantidad de confirmaciones necesarias para aceptar
// una bradicardia o taquicardia nueva.
const byte CONFIRMACIONES_CANDIDATO = 3;

// Máximo cambio visible por actualización.
const int PASO_MAXIMO_BPM = 4;

void reiniciarBPM()
{
    bpmEstable = -1;

    indiceMaxim = 0;
    cantidadMaxim = 0;

    ultimaActualizacionBPM = 0;

    cantidadInicial = 0;

    bpmCandidato = -1;
    repeticionesCandidato = 0;

    for (byte i = 0;
         i < TOTAL_RESULTADOS_BPM;
         i++)
    {
        historialMaxim[i] = 0;
    }

    for (byte i = 0; i < 3; i++)
    {
        historialInicial[i] = 0;
    }
}

// ================================================================
// INICIALIZACION
// ================================================================

bool MAX30102_begin()
{
    if (!sensor.begin(Wire, I2C_SPEED_FAST))
    {
        return false;
    }

    byte brillo = 60;
    byte promedio = 4;
    byte modoLED = 2;
    int frecuencia = 400;
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

    reiniciarBPM();

    return true;
}

// ================================================================
// ACTUALIZAR BPM DESDE LA SEÑAL IR
// ================================================================

/*void actualizarBPM(uint32_t valorIR, unsigned long ahora)
{
    if (!checkForBeat(valorIR))
    {
        // Si pasan más de 3 segundos sin pulso, invalida el BPM.
        if (tiempoUltimoLatido > 0 &&
            ahora - tiempoUltimoLatido > 3000)
        {
            reiniciarBPM();
        }

        return;
    }

    // El primer latido solamente establece una referencia.
    if (tiempoUltimoLatido == 0)
    {
        tiempoUltimoLatido = ahora;
        return;
    }

    unsigned long intervalo =
        ahora - tiempoUltimoLatido;

    tiempoUltimoLatido = ahora;

    if (intervalo == 0)
    {
        return;
    }

    float bpmInstantaneo =
        60000.0f / intervalo;

    // Aceptar solamente valores fisiológicamente razonables.
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
 }*/

// ================================================================
// ACTUALIZACION DEL SENSOR
// ================================================================

int calcularBPMRobusto()
{
    int copia[TOTAL_RESULTADOS_BPM];

    for (byte i = 0; i < TOTAL_RESULTADOS_BPM; i++)
    {
        copia[i] = historialMaxim[i];
    }

    // Ordenar de menor a mayor.
    for (byte i = 0;
         i < TOTAL_RESULTADOS_BPM - 1;
         i++)
    {
        for (byte j = i + 1;
             j < TOTAL_RESULTADOS_BPM;
             j++)
        {
            if (copia[j] < copia[i])
            {
                int temporal = copia[i];
                copia[i] = copia[j];
                copia[j] = temporal;
            }
        }
    }

    /*
     * Descartar los dos menores y los dos mayores.
     * Promediar las cinco mediciones centrales.
     */
    int suma = 0;

    for (byte i = 2; i <= 6; i++)
    {
        suma += copia[i];
    }

    return suma / 5;
}

int acercarBPM(
    int valorActual,
    int valorObjetivo
)
{
    int diferencia =
        valorObjetivo - valorActual;

    if (diferencia > PASO_MAXIMO_BPM)
    {
        return valorActual +
               PASO_MAXIMO_BPM;
    }

    if (diferencia < -PASO_MAXIMO_BPM)
    {
        return valorActual -
               PASO_MAXIMO_BPM;
    }

    return valorObjetivo;
}

void procesarBPMMaxim(
    int bpmNuevo,
    unsigned long ahora
)
{
    // Rechazar valores físicamente imposibles
    // para el rango definido.
    if (bpmNuevo < BPM_MINIMO_VALIDO ||
        bpmNuevo > BPM_MAXIMO_VALIDO)
    {
        return;
    }

    // Guardar nuevo resultado Maxim.
    historialMaxim[indiceMaxim] =
        bpmNuevo;

    indiceMaxim =
        (indiceMaxim + 1) %
        TOTAL_RESULTADOS_BPM;

    if (cantidadMaxim <
        TOTAL_RESULTADOS_BPM)
    {
        cantidadMaxim++;
    }

    // Esperar nueve resultados antes de calcular.
    if (cantidadMaxim <
        TOTAL_RESULTADOS_BPM)
    {
        return;
    }

    // Actualizar como máximo una vez por segundo.
    if (ahora - ultimaActualizacionBPM <
        1000)
    {
        return;
    }

    ultimaActualizacionBPM = ahora;

    int bpmCalculado =
        calcularBPMRobusto();

    // ============================================================
    // FASE INICIAL DE ESTABILIZACIÓN
    // ============================================================

    if (bpmEstable == -1)
    {
        historialInicial[cantidadInicial] =
            bpmCalculado;

        cantidadInicial++;

        // Necesitamos tres resultados robustos.
        if (cantidadInicial < 3)
        {
            return;
        }

        int minimoInicial =
            historialInicial[0];

        int maximoInicial =
            historialInicial[0];

        int sumaInicial = 0;

        for (byte i = 0; i < 3; i++)
        {
            sumaInicial +=
                historialInicial[i];

            if (historialInicial[i] <
                minimoInicial)
            {
                minimoInicial =
                    historialInicial[i];
            }

            if (historialInicial[i] >
                maximoInicial)
            {
                maximoInicial =
                    historialInicial[i];
            }
        }

        /*
         * Mostrar el primer BPM solamente si
         * las tres ventanas son coherentes.
         */
        if (maximoInicial -
            minimoInicial <=
            TOLERANCIA_INICIAL)
        {
            bpmEstable =
                sumaInicial / 3;

            cantidadInicial = 0;

            bpmCandidato = -1;
            repeticionesCandidato = 0;
        }
        else
        {
            /*
             * Las primeras ventanas no fueron
             * coherentes. Conservar las dos últimas
             * para volver a comprobar.
             */
            historialInicial[0] =
                historialInicial[1];

            historialInicial[1] =
                historialInicial[2];

            cantidadInicial = 2;
        }

        return;
    }

    // ============================================================
    // ACTUALIZACIÓN NORMAL
    // ============================================================

    int diferencia =
        abs(
            bpmCalculado -
            bpmEstable
        );

    if (diferencia <=
        CAMBIO_NORMAL_MAXIMO)
    {
        // Seguir cambios pequeños gradualmente.
        bpmEstable =
            acercarBPM(
                bpmEstable,
                bpmCalculado
            );

        bpmCandidato = -1;
        repeticionesCandidato = 0;

        return;
    }

    // ============================================================
    // CAMBIO GRANDE: CONFIRMARLO
    // ============================================================

    if (bpmCandidato == -1 ||
        abs(
            bpmCalculado -
            bpmCandidato
        ) >
        TOLERANCIA_CANDIDATO)
    {
        bpmCandidato =
            bpmCalculado;

        repeticionesCandidato = 1;

        return;
    }

    repeticionesCandidato++;

    if (repeticionesCandidato >=
        CONFIRMACIONES_CANDIDATO)
    {
        /*
         * El cambio grande se ha repetido.
         * Puede ser una variación real.
         *
         * No saltamos inmediatamente:
         * avanzamos progresivamente.
         */
        bpmEstable =
            acercarBPM(
                bpmEstable,
                bpmCandidato
            );

        /*
         * Si todavía estamos lejos del candidato,
         * conservarlo para seguir acercándonos.
         */
        if (abs(
                bpmEstable -
                bpmCandidato
            ) <=
            CAMBIO_NORMAL_MAXIMO)
        {
            bpmCandidato = -1;
            repeticionesCandidato = 0;
        }
        else
        {
            repeticionesCandidato =
                CONFIRMACIONES_CANDIDATO;
        }
    }
}

void MAX30102_update()
{
    unsigned long ahora = millis();

    // Con promedio 4 y frecuencia 100, se obtiene
    // aproximadamente una muestra efectiva cada 40 ms.
    /*if (ahora - ultimaMuestra < 40)
    {
        return;
    }

    ultimaMuestra = ahora;*/

    sensor.check();

    if (!sensor.available())
    {
        return;
    }

    uint32_t valorRojo = sensor.getRed();
    uint32_t valorIR = sensor.getIR();

    //Serial.print("IR = ");
    //Serial.println(valorIR);

    sensor.nextSample();

    // ============================================================
    // DETECCION DEL DEDO
    // ============================================================

    if (valorIR > UMBRAL_DEDO)
    {
        dedoDetectado = true;
        ultimoDedoDetectado = ahora;
    }
    else if (ahora - ultimoDedoDetectado > 500)
    {
        dedoDetectado = false;

        spo2Estable = -1;
        spo2Valido = 0;

        indiceMuestra = 0;

        reiniciarBPM();

        return;
    }

    if (!dedoDetectado)
    {
        return;
    }

    // ============================================================
    // CALCULAR BPM CON LA SEÑAL IR
    // ============================================================

    //actualizarBPM(valorIR, ahora);

    // ============================================================
    // LLENAR BUFER PARA SpO2
    // ============================================================

    redBuffer[indiceMuestra] = valorRojo;
    irBuffer[indiceMuestra] = valorIR;

    indiceMuestra++;

    if (indiceMuestra >= BUFFER_SIZE)
    {
        maxim_heart_rate_and_oxygen_saturation(
            irBuffer,
            BUFFER_SIZE,
            redBuffer,
            &spo2,
            &spo2Valido,
            &heartRateMaxim,
            &hrMaximValido
        );

        // ============================================================
        // CORREGIR BPM DEL ALGORITMO MAXIM
        // ============================================================

        // El sensor está configurado con promedio de 4.
        // Por eso la salida efectiva es aproximadamente 25 muestras/s,
        // mientras el algoritmo Maxim supone 100 muestras/s.
        
        if (hrMaximValido)
{
    procesarBPMMaxim(
        heartRateMaxim, ahora
    );

    Serial.print("BPM Maxim directo: ");
    Serial.print(heartRateMaxim);

    Serial.print(" | BPM filtrado: ");
    Serial.println(bpmEstable);
}

        if (spo2Valido &&
            spo2 >= 70 &&
            spo2 <= 100)
        {
            spo2Estable = spo2;
        }
        else
        {
            spo2Estable = -1;
        }

        // Mantener las últimas 75 muestras para crear
        // una ventana superpuesta.
        for (int i = 25; i < BUFFER_SIZE; i++)
        {
            irBuffer[i - 25] = irBuffer[i];
            redBuffer[i - 25] = redBuffer[i];
        }

        indiceMuestra =
            BUFFER_SIZE - 25;
    }
}

// ================================================================
// FUNCIONES DE SALIDA
// ================================================================

int MAX30102_getSpO2()
{
    if (!dedoDetectado)
    {
        return -1;
    }

    return spo2Estable;
}

int MAX30102_getHeartRate()
{
    if (!dedoDetectado)
    {
        return -1;
    }

    return bpmEstable;
}

bool MAX30102_hayDedo()
{
    return dedoDetectado;
}