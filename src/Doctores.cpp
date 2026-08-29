#include "Doctores.h"

#include "MicroSD.h"

#include <SD.h>

// ================================================================
// CONFIGURACION
// ================================================================

static const char* CARPETA_DOCTORES = "/DOCTORES";
static const char* ARCHIVO_DOCTORES = "/DOCTORES/doctores.txt";

// Separador utilizado dentro del archivo.
// Formato:
// id|nombre|usuario|contrasena
static const char SEPARADOR = '|';

// ================================================================
// SESION ACTUAL
// ================================================================

static bool autenticado = false;

static String doctorId = "";
static String doctorNombre = "";
static String doctorUsuario = "";

// ================================================================
// FUNCIONES INTERNAS
// ================================================================

static void limpiarSesion()
{
    autenticado = false;

    doctorId = "";
    doctorNombre = "";
    doctorUsuario = "";
}

static String limpiarCampo(String valor)
{
    valor.trim();

    // Evita romper el formato del archivo.
    valor.replace("|", " ");
    valor.replace("\n", " ");
    valor.replace("\r", " ");

    return valor;
}

static bool usuarioValido(const String& usuario)
{
    if (usuario.length() < 3 || usuario.length() > 30)
    {
        return false;
    }

    for (size_t i = 0; i < usuario.length(); i++)
    {
        char c = usuario[i];

        bool permitido =
            isAlphaNumeric(c) ||
            c == '_' ||
            c == '-' ||
            c == '.';

        if (!permitido)
        {
            return false;
        }
    }

    return true;
}

static bool contrasenaValida(const String& contrasena)
{
    return contrasena.length() >= 4 &&
           contrasena.length() <= 40;
}

static String generarDoctorId()
{
    unsigned long numero = 1;

    if (!SD.exists(ARCHIVO_DOCTORES))
    {
        return "doctor_001";
    }

    File archivo = SD.open(ARCHIVO_DOCTORES, FILE_READ);

    if (!archivo)
    {
        return "doctor_001";
    }

    while (archivo.available())
    {
        String linea = archivo.readStringUntil('\n');
        linea.trim();

        if (!linea.isEmpty())
        {
            numero++;
        }
    }

    archivo.close();

    char id[20];

    snprintf(
        id,
        sizeof(id),
        "doctor_%03lu",
        numero
    );

    return String(id);
}

static bool extraerCampos(
    const String& linea,
    String& id,
    String& nombre,
    String& usuario,
    String& contrasena
)
{
    int p1 = linea.indexOf(SEPARADOR);

    if (p1 < 0)
    {
        return false;
    }

    int p2 = linea.indexOf(SEPARADOR, p1 + 1);

    if (p2 < 0)
    {
        return false;
    }

    int p3 = linea.indexOf(SEPARADOR, p2 + 1);

    if (p3 < 0)
    {
        return false;
    }

    id = linea.substring(0, p1);
    nombre = linea.substring(p1 + 1, p2);
    usuario = linea.substring(p2 + 1, p3);
    contrasena = linea.substring(p3 + 1);

    id.trim();
    nombre.trim();
    usuario.trim();
    contrasena.trim();

    return !id.isEmpty() &&
           !nombre.isEmpty() &&
           !usuario.isEmpty();
}

// ================================================================
// INICIALIZACION
// ================================================================

bool Doctor_begin()
{
    limpiarSesion();

    if (!MicroSD_disponible())
    {
        Serial.println(
            "ERROR: no se puede iniciar Doctores sin MicroSD"
        );

        return false;
    }

    if (!SD.exists(CARPETA_DOCTORES))
    {
        if (!SD.mkdir(CARPETA_DOCTORES))
        {
            Serial.println(
                "ERROR: no se pudo crear /DOCTORES"
            );

            return false;
        }
    }

    if (!SD.exists(ARCHIVO_DOCTORES))
    {
        File archivo =
            SD.open(ARCHIVO_DOCTORES, FILE_WRITE);

        if (!archivo)
        {
            Serial.println(
                "ERROR: no se pudo crear doctores.txt"
            );

            return false;
        }

        archivo.close();

        Serial.println(
            "Archivo de doctores creado"
        );
    }

    Serial.println(
        "Modulo Doctores listo"
    );

    return true;
}

// ================================================================
// COMPROBAR USUARIO
// ================================================================

bool Doctor_usuarioExiste(const String& usuarioBuscado)
{
    if (!MicroSD_disponible())
    {
        return false;
    }

    if (!SD.exists(ARCHIVO_DOCTORES))
    {
        return false;
    }

    String usuarioNormalizado =
        usuarioBuscado;

    usuarioNormalizado.trim();
    usuarioNormalizado.toLowerCase();

    File archivo =
        SD.open(ARCHIVO_DOCTORES, FILE_READ);

    if (!archivo)
    {
        return false;
    }

    while (archivo.available())
    {
        String linea =
            archivo.readStringUntil('\n');

        linea.trim();

        if (linea.isEmpty())
        {
            continue;
        }

        String id;
        String nombre;
        String usuario;
        String contrasena;

        if (!extraerCampos(
                linea,
                id,
                nombre,
                usuario,
                contrasena))
        {
            continue;
        }

        usuario.toLowerCase();

        if (usuario == usuarioNormalizado)
        {
            archivo.close();
            return true;
        }
    }

    archivo.close();

    return false;
}

// ================================================================
// REGISTRAR DOCTOR
// ================================================================

bool Doctor_registrar(
    const String& nombreRecibido,
    const String& usuarioRecibido,
    const String& contrasenaRecibida
)
{
    if (!MicroSD_disponible())
    {
        Serial.println(
            "ERROR: MicroSD no disponible"
        );

        return false;
    }

    String nombre =
        limpiarCampo(nombreRecibido);

    String usuario =
        limpiarCampo(usuarioRecibido);

    String contrasena =
        limpiarCampo(contrasenaRecibida);

    usuario.toLowerCase();

    if (nombre.length() < 3 ||
        nombre.length() > 60)
    {
        Serial.println(
            "ERROR: nombre de doctor invalido"
        );

        return false;
    }

    if (!usuarioValido(usuario))
    {
        Serial.println(
            "ERROR: usuario invalido"
        );

        return false;
    }

    if (!contrasenaValida(contrasena))
    {
        Serial.println(
            "ERROR: contrasena invalida"
        );

        return false;
    }

    if (Doctor_usuarioExiste(usuario))
    {
        Serial.println(
            "ERROR: el usuario ya existe"
        );

        return false;
    }

    String id =
        generarDoctorId();

    File archivo =
        SD.open(ARCHIVO_DOCTORES, FILE_APPEND);

    if (!archivo)
    {
        Serial.println(
            "ERROR: no se pudo abrir doctores.txt"
        );

        return false;
    }

    archivo.print(id);
    archivo.print(SEPARADOR);

    archivo.print(nombre);
    archivo.print(SEPARADOR);

    archivo.print(usuario);
    archivo.print(SEPARADOR);

    archivo.println(contrasena);

    archivo.close();

    Serial.println(
        "Doctor registrado correctamente"
    );

    Serial.println(
        "ID: " + id
    );

    Serial.println(
        "Usuario: " + usuario
    );

    return true;
}

// ================================================================
// INICIAR SESION
// ================================================================

bool Doctor_iniciarSesion(
    const String& usuarioRecibido,
    const String& contrasenaRecibida
)
{
    limpiarSesion();

    if (!MicroSD_disponible())
    {
        Serial.println(
            "ERROR: MicroSD no disponible"
        );

        return false;
    }

    if (!SD.exists(ARCHIVO_DOCTORES))
    {
        Serial.println(
            "ERROR: archivo de doctores no existe"
        );

        return false;
    }

    String usuarioBuscado =
        usuarioRecibido;

    String contrasenaBuscada =
        contrasenaRecibida;

    usuarioBuscado.trim();
    contrasenaBuscada.trim();

    usuarioBuscado.toLowerCase();

    File archivo =
        SD.open(ARCHIVO_DOCTORES, FILE_READ);

    if (!archivo)
    {
        Serial.println(
            "ERROR: no se pudo abrir doctores.txt"
        );

        return false;
    }

    while (archivo.available())
    {
        String linea =
            archivo.readStringUntil('\n');

        linea.trim();

        if (linea.isEmpty())
        {
            continue;
        }

        String id;
        String nombre;
        String usuario;
        String contrasena;

        if (!extraerCampos(
                linea,
                id,
                nombre,
                usuario,
                contrasena))
        {
            continue;
        }

        String usuarioComparar =
            usuario;

        usuarioComparar.toLowerCase();

        if (usuarioComparar == usuarioBuscado &&
            contrasena == contrasenaBuscada)
        {
            autenticado = true;

            doctorId = id;
            doctorNombre = nombre;
            doctorUsuario = usuario;

            archivo.close();

            Serial.println(
                "Inicio de sesion correcto"
            );

            Serial.println(
                "Doctor: " + doctorNombre
            );

            Serial.println(
                "ID: " + doctorId
            );

            return true;
        }
    }

    archivo.close();

    Serial.println(
        "ERROR: credenciales incorrectas"
    );

    return false;
}

// ================================================================
// CERRAR SESION
// ================================================================

void Doctor_cerrarSesion()
{
    limpiarSesion();

    Serial.println(
        "Sesion del doctor cerrada"
    );
}

// ================================================================
// CONSULTAS
// ================================================================

bool Doctor_autenticado()
{
    return autenticado;
}

String Doctor_getId()
{
    return doctorId;
}

String Doctor_getNombre()
{
    return doctorNombre;
}

String Doctor_getUsuario()
{
    return doctorUsuario;
}