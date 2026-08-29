#include "Pacientes.h"
#include "MicroSD.h"

#include <SD.h>

// ================================================================
// UTILIDADES
// ================================================================

static bool cedulaValida(const String& cedula)
{
    if (cedula.length() < 6 ||
        cedula.length() > 15)
    {
        return false;
    }

    for (size_t i = 0; i < cedula.length(); i++)
    {
        if (!isDigit(cedula[i]))
        {
            return false;
        }
    }

    return true;
}

static String limpiarCampo(String valor)
{
    valor.trim();

    valor.replace("\n", " ");
    valor.replace("\r", " ");
    valor.replace("=", " ");

    return valor;
}

static String escaparJSON(String valor)
{
    valor.replace("\\", "\\\\");
    valor.replace("\"", "\\\"");
    valor.replace("\n", " ");
    valor.replace("\r", " ");

    return valor;
}

static String leerValor(
    const String& ruta,
    const String& clave
)
{
    File archivo = SD.open(ruta, FILE_READ);

    if (!archivo)
    {
        return "";
    }

    const String prefijo = clave + "=";

    while (archivo.available())
    {
        String linea =
            archivo.readStringUntil('\n');

        linea.trim();

        if (linea.startsWith(prefijo))
        {
            archivo.close();

            String valor =
                linea.substring(prefijo.length());

            valor.trim();

            return valor;
        }
    }

    archivo.close();

    return "";
}

// ================================================================
// GUARDAR PACIENTE
// ================================================================

bool Paciente_guardar(
    const String& nombre,
    const String& cedula,
    int edad,
    const String& sexo,
    const String& observaciones
)
{
    return Paciente_guardar(
        nombre,
        cedula,
        edad,
        sexo,
        observaciones,
        "",
        ""
    );
}

bool Paciente_guardar(
    const String& nombreRecibido,
    const String& cedulaRecibida,
    int edad,
    const String& sexoRecibido,
    const String& observacionesRecibidas,
    const String& doctorIdRecibido,
    const String& doctorNombreRecibido
)
{
    if (!MicroSD_disponible())
    {
        return false;
    }

    String nombre =
        limpiarCampo(nombreRecibido);

    String cedula =
        limpiarCampo(cedulaRecibida);

    String sexo =
        limpiarCampo(sexoRecibido);

    String observaciones =
        limpiarCampo(observacionesRecibidas);

    String doctorId =
        limpiarCampo(doctorIdRecibido);

    String doctorNombre =
        limpiarCampo(doctorNombreRecibido);

    if (nombre.isEmpty() ||
        !cedulaValida(cedula) ||
        edad < 1 ||
        edad > 120 ||
        sexo.isEmpty())
    {
        return false;
    }

    if (!SD.exists("/PACIENTES"))
    {
        if (!SD.mkdir("/PACIENTES"))
        {
            return false;
        }
    }

    String carpeta =
        "/PACIENTES/" + cedula;

    if (!SD.exists(carpeta))
    {
        if (!SD.mkdir(carpeta))
        {
            return false;
        }
    }

    String ruta =
        carpeta + "/paciente.txt";

    if (SD.exists(ruta))
    {
        SD.remove(ruta);
    }

    File archivo =
        SD.open(ruta, FILE_WRITE);

    if (!archivo)
    {
        return false;
    }

    archivo.println(
        "DoctorID=" + doctorId
    );

    archivo.println(
        "DoctorNombre=" + doctorNombre
    );

    archivo.println(
        "Nombre=" + nombre
    );

    archivo.println(
        "Cedula=" + cedula
    );

    archivo.println(
        "Edad=" + String(edad)
    );

    archivo.println(
        "Sexo=" + sexo
    );

    archivo.println(
        "Observaciones=" + observaciones
    );

    // Estado médico del registro.
    archivo.println(
        "Estado=pendiente"
    );

    // Estado de sincronización para usarlo después.
    archivo.println(
        "Sincronizacion=pendiente"
    );

    archivo.close();

    Serial.println(
        "Paciente guardado: " + ruta
    );

    return true;
}

// ================================================================
// LISTAR PACIENTES DEL DOCTOR
// ================================================================

String Paciente_listarPorDoctorJSON(
    const String& doctorIdBuscado
)
{
    String json =
        "{\"ok\":true,\"pacientes\":[";

    if (!MicroSD_disponible() ||
        !SD.exists("/PACIENTES"))
    {
        json += "]}";
        return json;
    }

    File carpetaPacientes =
        SD.open("/PACIENTES");

    if (!carpetaPacientes ||
        !carpetaPacientes.isDirectory())
    {
        json += "]}";
        return json;
    }

    bool primero = true;

    File entrada =
        carpetaPacientes.openNextFile();

    while (entrada)
    {
        if (entrada.isDirectory())
        {
            String nombreCarpeta =
                entrada.name();

            // Algunas versiones entregan solo la carpeta y otras
            // entregan la ruta completa.
            int ultimaBarra =
                nombreCarpeta.lastIndexOf('/');

            String cedulaCarpeta =
                ultimaBarra >= 0
                    ? nombreCarpeta.substring(
                          ultimaBarra + 1
                      )
                    : nombreCarpeta;

            String ruta =
                "/PACIENTES/" +
                cedulaCarpeta +
                "/paciente.txt";

            if (SD.exists(ruta))
            {
                String doctorId =
                    leerValor(ruta, "DoctorID");

                if (doctorId == doctorIdBuscado)
                {
                    String nombre =
                        leerValor(ruta, "Nombre");

                    String cedula =
                        leerValor(ruta, "Cedula");

                    String edad =
                        leerValor(ruta, "Edad");

                    String sexo =
                        leerValor(ruta, "Sexo");

                    String observaciones =
                        leerValor(
                            ruta,
                            "Observaciones"
                        );

                    String estado =
                        leerValor(ruta, "Estado");

                    String sincronizacion =
                        leerValor(
                            ruta,
                            "Sincronizacion"
                        );

                    // Compatibilidad con pacientes antiguos.
                    if (estado.isEmpty())
                    {
                        estado = "pendiente";
                    }

                    if (sincronizacion.isEmpty())
                    {
                        sincronizacion = "pendiente";
                    }

                    if (!primero)
                    {
                        json += ",";
                    }

                    primero = false;

                    json += "{";

                    json += "\"nombre\":\"" +
                        escaparJSON(nombre) +
                        "\",";

                    json += "\"cedula\":\"" +
                        escaparJSON(cedula) +
                        "\",";

                    json += "\"edad\":" +
                        String(edad.toInt()) +
                        ",";

                    json += "\"sexo\":\"" +
                        escaparJSON(sexo) +
                        "\",";

                    json += "\"observaciones\":\"" +
                        escaparJSON(observaciones) +
                        "\",";

                    json += "\"estado\":\"" +
                        escaparJSON(estado) +
                        "\",";

                    json += "\"sincronizacion\":\"" +
                        escaparJSON(sincronizacion) +
                        "\"";

                    json += "}";
                }
            }
        }

        entrada.close();

        entrada =
            carpetaPacientes.openNextFile();
    }

    carpetaPacientes.close();

    json += "]}";

    return json;
}

// ================================================================
// MARCAR PACIENTE COMO REVISADO
// ================================================================

bool Paciente_marcarRevisado(
    const String& cedula,
    const String& doctorId
)
{
    if (!MicroSD_disponible() ||
        !cedulaValida(cedula))
    {
        return false;
    }

    String ruta =
        "/PACIENTES/" +
        cedula +
        "/paciente.txt";

    if (!SD.exists(ruta))
    {
        return false;
    }

    // Evita que un doctor modifique pacientes de otro.
    String propietario =
        leerValor(ruta, "DoctorID");

    if (propietario != doctorId)
    {
        return false;
    }

    File original =
        SD.open(ruta, FILE_READ);

    if (!original)
    {
        return false;
    }

    String contenido = "";

    while (original.available())
    {
        String linea =
            original.readStringUntil('\n');

        linea.trim();

        if (linea.startsWith("Estado="))
        {
            linea = "Estado=revisado";
        }

        contenido += linea;
        contenido += "\n";
    }

    original.close();

    // Pacientes antiguos podrían no tener Estado.
    if (contenido.indexOf("Estado=") < 0)
    {
        contenido += "Estado=revisado\n";
    }

    String temporal =
        "/PACIENTES/" +
        cedula +
        "/paciente.tmp";

    if (SD.exists(temporal))
    {
        SD.remove(temporal);
    }

    File nuevo =
        SD.open(temporal, FILE_WRITE);

    if (!nuevo)
    {
        return false;
    }

    nuevo.print(contenido);
    nuevo.close();

    SD.remove(ruta);

    if (!SD.rename(temporal, ruta))
    {
        return false;
    }

    Serial.println(
        "Paciente marcado como revisado: " +
        cedula
    );

    return true;
}