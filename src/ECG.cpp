#include "ECG.h"
#include "Config.h"

// ================================================================
// CONFIGURACIÓN
// ================================================================

// Déjalo en true si el pico principal aparece hacia abajo.
// El cable nuevo mostró deflexiones negativas, por eso inicialmente
// conviene mantenerlo en true.
static const bool INVERTIR_SENAL = true;

// Centro visual del ADC de 12 bits.
static const int CENTRO_ECG = 2048;

// Límites válidos del ADC.
static const int ADC_MINIMO_VALIDO = 5;
static const int ADC_MAXIMO_VALIDO = 4090;

// Ganancia visual aplicada después del filtrado.
// Si la gráfica queda demasiado alta, baja a 1.2.
// Si queda muy pequeña, sube gradualmente hasta 2.0.
static const float GANANCIA_VISUAL = 1.45f;

// Velocidad con la que se actualiza la línea base.
// Un valor pequeño elimina la deriva lenta sin borrar el latido.
static const float ALPHA_LINEA_BASE = 0.006f;

// Suavizado de ruido rápido.
// Más bajo = más suave, pero con mayor retraso.
// Más alto = responde más rápido, pero deja más ruido.
static const float ALPHA_SUAVIZADO = 0.32f;

// Cantidad de lecturas consecutivas necesarias para confirmar
// desconexión o reconexión de los electrodos.
static const uint8_t MUESTRAS_DESCONEXION = 5;
static const uint8_t MUESTRAS_RECONEXION = 10;

// Cantidad de muestras saturadas consecutivas para invalidar la señal.
static const uint8_t MUESTRAS_SATURACION = 5;

// ================================================================
// VARIABLES INTERNAS
// ================================================================

static int ecgValue = CENTRO_ECG;

static bool electrodosDesconectados = true;
static bool filtroInicializado = false;

static uint8_t contadorDesconexion = 0;
static uint8_t contadorReconexion = 0;
static uint8_t contadorSaturacion = 0;

// Promedio lento que representa la deriva de la línea base.
static float lineaBase = CENTRO_ECG;

// Señal después del suavizado.
static float senalSuavizada = 0.0f;

// ================================================================
// REINICIAR FILTRO
// ================================================================

static void reiniciarFiltro(float lecturaInicial)
{
    lineaBase = lecturaInicial;
    senalSuavizada = 0.0f;

    ecgValue = CENTRO_ECG;

    contadorSaturacion = 0;
    filtroInicializado = true;
}

// ================================================================
// DETECCIÓN ESTABLE DE ELECTRODOS
// ================================================================

static void actualizarEstadoElectrodos()
{
    bool desconexionInstantanea =
        digitalRead(pinLOPlus) == HIGH ||
        digitalRead(pinLOMinus) == HIGH;

    if (desconexionInstantanea)
    {
        contadorReconexion = 0;

        if (contadorDesconexion < MUESTRAS_DESCONEXION)
        {
            contadorDesconexion++;
        }

        if (contadorDesconexion >= MUESTRAS_DESCONEXION)
        {
            electrodosDesconectados = true;
            filtroInicializado = false;
        }
    }
    else
    {
        contadorDesconexion = 0;

        if (contadorReconexion < MUESTRAS_RECONEXION)
        {
            contadorReconexion++;
        }

        if (contadorReconexion >= MUESTRAS_RECONEXION)
        {
            electrodosDesconectados = false;
        }
    }
}

// ================================================================
// INICIALIZACIÓN
// ================================================================

void ECG_begin()
{
    pinMode(pinECG, INPUT);

    pinMode(pinLOPlus, INPUT);
    pinMode(pinLOMinus, INPUT);

    analogReadResolution(12);
    analogSetPinAttenuation(
        pinECG,
        ADC_11db
    );

    ecgValue = CENTRO_ECG;
    electrodosDesconectados = true;

    contadorDesconexion = 0;
    contadorReconexion = 0;
    contadorSaturacion = 0;

    lineaBase = CENTRO_ECG;
    senalSuavizada = 0.0f;

    filtroInicializado = false;
}

// ================================================================
// ACTUALIZACIÓN ECG
// Debe llamarse aproximadamente cada 10 ms.
// ================================================================

void ECG_update()
{
    actualizarEstadoElectrodos();

    // Mientras algún electrodo esté desconectado,
    // entregar una línea recta centrada.
    if (electrodosDesconectados)
    {
        ecgValue = CENTRO_ECG;
        return;
    }

    // ------------------------------------------------------------
    // 1. Promediar cuatro lecturas del ADC
    // ------------------------------------------------------------

    uint32_t suma = 0;

    for (uint8_t i = 0; i < 4; i++)
    {
        suma += analogRead(pinECG);
    }

    float lecturaCruda =
        static_cast<float>(suma) / 4.0f;

    // ------------------------------------------------------------
    // 2. Comprobar saturación del ADC
    // ------------------------------------------------------------

    if (lecturaCruda <= ADC_MINIMO_VALIDO ||
        lecturaCruda >= ADC_MAXIMO_VALIDO)
    {
        if (contadorSaturacion < MUESTRAS_SATURACION)
        {
            contadorSaturacion++;
        }

        if (contadorSaturacion >= MUESTRAS_SATURACION)
        {
            ecgValue = CENTRO_ECG;
            filtroInicializado = false;
        }

        return;
    }

    contadorSaturacion = 0;

    // ------------------------------------------------------------
    // 3. Invertir para mostrar el complejo principal hacia arriba
    // ------------------------------------------------------------

    if (INVERTIR_SENAL)
    {
        lecturaCruda =
            4095.0f - lecturaCruda;
    }

    // Al conectar los electrodos, inicializar sin provocar
    // un salto grande en la gráfica.
    if (!filtroInicializado)
    {
        reiniciarFiltro(
            lecturaCruda
        );

        return;
    }

    // ------------------------------------------------------------
    // 4. Estimar la línea base
    // ------------------------------------------------------------

    lineaBase +=
        ALPHA_LINEA_BASE *
        (lecturaCruda - lineaBase);

    // Eliminar el desplazamiento lento.
    float senalCentrada =
        lecturaCruda - lineaBase;

    // ------------------------------------------------------------
    // 5. Suavizar ruido rápido
    // ------------------------------------------------------------

    senalSuavizada +=
        ALPHA_SUAVIZADO *
        (senalCentrada - senalSuavizada);

    // ------------------------------------------------------------
    // 6. Centrar y escalar para la página web
    // ------------------------------------------------------------

    float salidaVisual =
        CENTRO_ECG +
        (senalSuavizada * GANANCIA_VISUAL);

    salidaVisual =
        constrain(
            salidaVisual,
            0.0f,
            4095.0f
        );

    ecgValue =
        static_cast<int>(salidaVisual);
}

// ================================================================
// SALIDAS PÚBLICAS
// ================================================================

// Los BPM se calculan mediante el MAX30102.
int ECG_getBPM()
{
    return 0;
}

int ECG_getValue()
{
    return ecgValue;
}

// El AD8232 ya no controla el indicador del latido.
bool ECG_getBeat()
{
    return false;
}

bool ECG_electrodosDesconectados()
{
    return electrodosDesconectados;
}