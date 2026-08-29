#include "Sesiones.h"
#include "MicroSD.h"
#include "RTC.h"

#include <SD.h>

static bool sesionActiva = false;

static File archivoECG;
static File archivoDatos;

static String rutaSesionActual = "";

static unsigned long inicioSesion = 0;
static unsigned long ultimaMuestraECG = 0;
static unsigned long ultimoDatoClinico = 0;

// ================================================================
// FECHA Y HORA DE SESION
// ================================================================

static String fechaInicioSesion = "";
static String horaInicioSesion = "";

static String fechaFinSesion = "";
static String horaFinSesion = "";

// ================================================================
// ESTADISTICAS
// ================================================================

static long sumaBPM = 0;
static long sumaSpO2 = 0;

static int cantidadBPM = 0;
static int cantidadSpO2 = 0;

static int bpmMinimo = 999;
static int bpmMaximo = 0;

static int spo2Minimo = 999;
static int spo2Maximo = 0;

// ================================================================
// CREAR RUTA DE SESION
// ================================================================

static String crearRutaSesion(const String& cedula)
{
    String carpetaPaciente =
        "/PACIENTES/" + cedula;

    if (!SD.exists(carpetaPaciente))
    {
        return "";
    }

    for (int numero = 1;
         numero <= 999;
         numero++)
    {
        char nombreSesion[20];

        snprintf(
            nombreSesion,
            sizeof(nombreSesion),
            "/SESION_%03d",
            numero
        );

        String ruta =
            carpetaPaciente +
            String(nombreSesion);

        if (!SD.exists(ruta))
        {
            return ruta;
        }
    }

    return "";
}

// ================================================================
// INICIAR SESION
// ================================================================

bool Sesion_iniciar(
    const String& cedula
)
{
    if (sesionActiva)
    {
        Serial.println(
            "ERROR: Ya existe una sesion activa"
        );

        return false;
    }

    if (!MicroSD_disponible())
    {
        Serial.println(
            "ERROR: MicroSD no disponible"
        );

        return false;
    }

    if (cedula.length() == 0)
    {
        Serial.println(
            "ERROR: Cedula vacia"
        );

        return false;
    }

    rutaSesionActual =
        crearRutaSesion(cedula);

    if (rutaSesionActual.length() == 0)
    {
        Serial.println(
            "ERROR: No se pudo crear una nueva ruta de sesion"
        );

        return false;
    }

    if (!SD.mkdir(rutaSesionActual))
    {
        Serial.println(
            "ERROR: No se pudo crear la carpeta de sesion"
        );

        return false;
    }

    // ============================================================
    // FECHA Y HORA DE INICIO
    // ============================================================

    if (RTC_disponible())
    {
        fechaInicioSesion =
            RTC_getFechaArchivo();

        horaInicioSesion =
            RTC_getHoraArchivo();
    }
    else
    {
        fechaInicioSesion =
            "NO_DISPONIBLE";

        horaInicioSesion =
            "NO_DISPONIBLE";
    }

    fechaFinSesion = "";
    horaFinSesion = "";

    // ============================================================
    // ABRIR ARCHIVOS
    // ============================================================

    archivoECG = SD.open(
        rutaSesionActual +
        "/ecg.csv",
        FILE_WRITE
    );

    archivoDatos = SD.open(
        rutaSesionActual +
        "/datos.csv",
        FILE_WRITE
    );

    if (!archivoECG ||
        !archivoDatos)
    {
        Serial.println(
            "ERROR: No se pudieron abrir los archivos de sesion"
        );

        if (archivoECG)
        {
            archivoECG.close();
        }

        if (archivoDatos)
        {
            archivoDatos.close();
        }

        return false;
    }

    archivoECG.println(
        "Tiempo_ms,ECG"
    );

    archivoDatos.println(
        "Tiempo_ms,BPM,SpO2"
    );

    // ============================================================
    // REINICIAR TEMPORIZADORES
    // ============================================================

    inicioSesion = millis();

    ultimaMuestraECG = 0;
    ultimoDatoClinico = 0;

    // ============================================================
    // REINICIAR ESTADISTICAS
    // ============================================================

    sumaBPM = 0;
    sumaSpO2 = 0;

    cantidadBPM = 0;
    cantidadSpO2 = 0;

    bpmMinimo = 999;
    bpmMaximo = 0;

    spo2Minimo = 999;
    spo2Maximo = 0;

    sesionActiva = true;

    Serial.println(
        "Sesion iniciada"
    );

    Serial.println(
        "Ruta: " +
        rutaSesionActual
    );

    Serial.print(
        "Fecha inicio: "
    );

    Serial.println(
        fechaInicioSesion
    );

    Serial.print(
        "Hora inicio: "
    );

    Serial.println(
        horaInicioSesion
    );

    return true;
}

// ================================================================
// ACTUALIZAR SESION
// ================================================================

void Sesion_actualizar(
    int bpm,
    int spo2,
    int ecg
)
{
    if (!sesionActiva)
    {
        return;
    }

    unsigned long ahora =
        millis();

    unsigned long tiempoSesion =
        ahora - inicioSesion;

    // ============================================================
    // ECG CADA 10 ms
    // ============================================================

    if (ahora - ultimaMuestraECG >=
        10)
    {
        ultimaMuestraECG = ahora;

        archivoECG.print(
            tiempoSesion
        );

        archivoECG.print(",");

        archivoECG.println(
            ecg
        );
    }

    // ============================================================
    // DATOS CLINICOS CADA SEGUNDO
    // ============================================================

    if (ahora - ultimoDatoClinico >=
        1000)
    {
        ultimoDatoClinico = ahora;

        archivoDatos.print(
            tiempoSesion
        );

        archivoDatos.print(",");

        archivoDatos.print(
            bpm
        );

        archivoDatos.print(",");

        archivoDatos.println(
            spo2
        );

        // ========================================================
        // BPM
        // ========================================================

        if (bpm > 0)
        {
            sumaBPM += bpm;
            cantidadBPM++;

            if (bpm < bpmMinimo)
            {
                bpmMinimo = bpm;
            }

            if (bpm > bpmMaximo)
            {
                bpmMaximo = bpm;
            }
        }

        // ========================================================
        // SpO2
        // ========================================================

        if (spo2 > 0)
        {
            sumaSpO2 += spo2;
            cantidadSpO2++;

            if (spo2 < spo2Minimo)
            {
                spo2Minimo = spo2;
            }

            if (spo2 > spo2Maximo)
            {
                spo2Maximo = spo2;
            }
        }

        archivoECG.flush();
        archivoDatos.flush();
    }
}

// ================================================================
// FINALIZAR SESION
// ================================================================

bool Sesion_finalizar()
{
    if (!sesionActiva)
    {
        Serial.println(
            "ERROR: No existe una sesion activa"
        );

        return false;
    }

    unsigned long duracion =
        millis() - inicioSesion;

    // ============================================================
    // FECHA Y HORA DE FIN
    // ============================================================

    if (RTC_disponible())
    {
        fechaFinSesion =
            RTC_getFechaArchivo();

        horaFinSesion =
            RTC_getHoraArchivo();
    }
    else
    {
        fechaFinSesion =
            "NO_DISPONIBLE";

        horaFinSesion =
            "NO_DISPONIBLE";
    }

    // ============================================================
    // CERRAR CSV
    // ============================================================

    archivoECG.flush();
    archivoDatos.flush();

    archivoECG.close();
    archivoDatos.close();

    // ============================================================
    // CREAR RESUMEN
    // ============================================================

    File resumen = SD.open(
        rutaSesionActual +
        "/resumen.txt",
        FILE_WRITE
    );

    if (resumen)
    {
        resumen.println(
            "Duracion_ms=" +
            String(duracion)
        );

        // ========================================================
        // FECHA / HORA
        // ========================================================

        resumen.println(
            "Fecha_inicio=" +
            fechaInicioSesion
        );

        resumen.println(
            "Hora_inicio=" +
            horaInicioSesion
        );

        resumen.println(
            "Fecha_fin=" +
            fechaFinSesion
        );

        resumen.println(
            "Hora_fin=" +
            horaFinSesion
        );

        // ========================================================
        // BPM
        // ========================================================

        if (cantidadBPM > 0)
        {
            resumen.println(
                "BPM_promedio=" +
                String(
                    sumaBPM /
                    cantidadBPM
                )
            );

            resumen.println(
                "BPM_minimo=" +
                String(
                    bpmMinimo
                )
            );

            resumen.println(
                "BPM_maximo=" +
                String(
                    bpmMaximo
                )
            );
        }
        else
        {
            resumen.println(
                "BPM_promedio=Sin datos"
            );
        }

        // ========================================================
        // SpO2
        // ========================================================

        if (cantidadSpO2 > 0)
        {
            resumen.println(
                "SpO2_promedio=" +
                String(
                    sumaSpO2 /
                    cantidadSpO2
                )
            );

            resumen.println(
                "SpO2_minimo=" +
                String(
                    spo2Minimo
                )
            );

            resumen.println(
                "SpO2_maximo=" +
                String(
                    spo2Maximo
                )
            );
        }
        else
        {
            resumen.println(
                "SpO2_promedio=Sin datos"
            );
        }

        resumen.println(
            "Estado=PENDIENTE"
        );

        resumen.close();
    }
    else
    {
        Serial.println(
            "ERROR: No se pudo crear resumen.txt"
        );
    }

    sesionActiva = false;

    Serial.println(
        "Sesion finalizada"
    );

    Serial.println(
        "Ruta: " +
        rutaSesionActual
    );

    Serial.print(
        "Fecha fin: "
    );

    Serial.println(
        fechaFinSesion
    );

    Serial.print(
        "Hora fin: "
    );

    Serial.println(
        horaFinSesion
    );

    return true;
}

// ================================================================
// ESTADO
// ================================================================

bool Sesion_activa()
{
    return sesionActiva;
}

// ================================================================
// RUTA ACTUAL
// ================================================================

String Sesion_rutaActual()
{
    return rutaSesionActual;
}