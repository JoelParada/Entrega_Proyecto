#include "RTC.h"

#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

static bool rtcDisponible = false;

// ================================================================
// INICIALIZACION
// ================================================================

bool RTC_begin()
{
    rtcDisponible = rtc.begin();

    if (!rtcDisponible)
    {
        Serial.println(
            "ERROR: RTC DS3231 no detectado"
        );

        return false;
    }

    Serial.println(
        "RTC DS3231 listo"
    );

    if (rtc.lostPower())
    {
        Serial.println(
            "RTC perdio fecha y hora"
        );

        /*
         * Primera configuracion:
         * usa la fecha y hora con la que
         * se compilo el programa.
         */
        rtc.adjust(
            DateTime(
                F(__DATE__),
                F(__TIME__)
            )
        );

        Serial.println(
            "RTC ajustado con fecha/hora de compilacion"
        );
    }

    return true;
}

// ================================================================
// ESTADO
// ================================================================

bool RTC_disponible()
{
    return rtcDisponible;
}

// ================================================================
// FECHA
// ================================================================

String RTC_getFecha()
{
    if (!rtcDisponible)
    {
        return "";
    }

    DateTime ahora = rtc.now();

    char buffer[11];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d/%02d/%04d",
        ahora.day(),
        ahora.month(),
        ahora.year()
    );

    return String(buffer);
}

// ================================================================
// HORA
// ================================================================

String RTC_getHora()
{
    if (!rtcDisponible)
    {
        return "";
    }

    DateTime ahora = rtc.now();

    char buffer[9];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d:%02d",
        ahora.hour(),
        ahora.minute(),
        ahora.second()
    );

    return String(buffer);
}

// ================================================================
// FECHA Y HORA
// ================================================================

String RTC_getFechaHora()
{
    if (!rtcDisponible)
    {
        return "";
    }

    DateTime ahora = rtc.now();

    char buffer[20];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d/%02d/%04d %02d:%02d:%02d",
        ahora.day(),
        ahora.month(),
        ahora.year(),
        ahora.hour(),
        ahora.minute(),
        ahora.second()
    );

    return String(buffer);
}

// ================================================================
// FORMATOS UTILES PARA ARCHIVOS
// ================================================================

String RTC_getFechaArchivo()
{
    if (!rtcDisponible)
    {
        return "";
    }

    DateTime ahora = rtc.now();

    char buffer[11];

    snprintf(
        buffer,
        sizeof(buffer),
        "%04d-%02d-%02d",
        ahora.year(),
        ahora.month(),
        ahora.day()
    );

    return String(buffer);
}

String RTC_getHoraArchivo()
{
    if (!rtcDisponible)
    {
        return "";
    }

    DateTime ahora = rtc.now();

    char buffer[9];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d:%02d",
        ahora.hour(),
        ahora.minute(),
        ahora.second()
    );

    return String(buffer);
}

// ================================================================
// VALORES INDIVIDUALES
// ================================================================

int RTC_getDia()
{
    if (!rtcDisponible)
    {
        return -1;
    }

    return rtc.now().day();
}

int RTC_getMes()
{
    if (!rtcDisponible)
    {
        return -1;
    }

    return rtc.now().month();
}

int RTC_getAnio()
{
    if (!rtcDisponible)
    {
        return -1;
    }

    return rtc.now().year();
}

int RTC_getHoraNumero()
{
    if (!rtcDisponible)
    {
        return -1;
    }

    return rtc.now().hour();
}

int RTC_getMinuto()
{
    if (!rtcDisponible)
    {
        return -1;
    }

    return rtc.now().minute();
}

int RTC_getSegundo()
{
    if (!rtcDisponible)
    {
        return -1;
    }

    return rtc.now().second();
}