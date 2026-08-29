#include "Sincronizacion.h"
#include "MicroSD.h"
#include "CredencialesWiFi.h"
#include "CredencialesSupabase.h"

#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "lwip/dns.h"
#include "lwip/ip_addr.h"

static bool sincronizacionActiva = false;
static int registrosPendientes = 0;

static String nombreBase(const String& ruta)
{
    int p = ruta.lastIndexOf('/');
    return p >= 0 ? ruta.substring(p + 1) : ruta;
}

static String escaparJSON(String texto)
{
    texto.replace("\\", "\\\\");
    texto.replace("\"", "\\\"");
    texto.replace("\n", " ");
    texto.replace("\r", " ");
    return texto;
}

static String numeroJSON(const String& valor, const String& defecto = "null")
{
    String copia = valor;
    copia.trim();

    if (copia.isEmpty() || copia == "Sin datos" || copia == "sin datos" || copia == "--")
        return defecto;

    bool digito = false;
    bool punto = false;

    for (size_t i = 0; i < copia.length(); i++)
    {
        char c = copia[i];
        if (isDigit(c)) { digito = true; continue; }
        if (c == '-' && i == 0) continue;
        if (c == '.' && !punto) { punto = true; continue; }
        return defecto;
    }

    return digito ? copia : defecto;
}

String Sincronizacion_leerValor(const String& rutaArchivo, const String& clave)
{
    File archivo = SD.open(rutaArchivo, FILE_READ);
    if (!archivo) return "";

    String prefijo = clave + "=";

    while (archivo.available())
    {
        String linea = archivo.readStringUntil('\n');
        linea.trim();

        if (linea.startsWith(prefijo))
        {
            String valor = linea.substring(prefijo.length());
            valor.trim();
            archivo.close();
            return valor;
        }
    }

    archivo.close();
    return "";
}

static bool escribirValorArchivo(const String& rutaArchivo, const String& clave, const String& nuevoValor)
{
    if (!SD.exists(rutaArchivo)) return false;

    File original = SD.open(rutaArchivo, FILE_READ);
    if (!original) return false;

    String contenido = "";
    String prefijo = clave + "=";
    bool encontrada = false;

    while (original.available())
    {
        String linea = original.readStringUntil('\n');
        linea.trim();

        if (linea.startsWith(prefijo))
        {
            linea = prefijo + nuevoValor;
            encontrada = true;
        }

        contenido += linea + "\n";
    }

    original.close();

    if (!encontrada)
        contenido += prefijo + nuevoValor + "\n";

    String temporal = rutaArchivo + ".tmp";
    if (SD.exists(temporal)) SD.remove(temporal);

    File nuevo = SD.open(temporal, FILE_WRITE);
    if (!nuevo) return false;

    nuevo.print(contenido);
    nuevo.close();

    if (!SD.remove(rutaArchivo))
    {
        SD.remove(temporal);
        return false;
    }

    return SD.rename(temporal, rutaArchivo);
}

static void forzarServidoresDNS()
{
    ip_addr_t dnsGoogle;
    ip_addr_t dnsCloudflare;

    IP_ADDR4(
        &dnsGoogle,
        8,
        8,
        8,
        8
    );

    IP_ADDR4(
        &dnsCloudflare,
        1,
        1,
        1,
        1
    );

    dns_setserver(
        0,
        &dnsGoogle
    );

    dns_setserver(
        1,
        &dnsCloudflare
    );

    Serial.println(
        "DNS forzado manualmente"
    );

    Serial.print(
        "DNS 1: "
    );

    Serial.println(
        WiFi.dnsIP(0)
    );

    Serial.print(
        "DNS 2: "
    );

    Serial.println(
        WiFi.dnsIP(1)
    );
}

static bool probarDNSSupabase()
{
    IPAddress ipSupabase;

    Serial.print("Resolviendo DNS de: ");
    Serial.println(SUPABASE_HOST);

    for (int intento = 1; intento <= 4; intento++)
    {
        Serial.print("Intento DNS ");
        Serial.print(intento);
        Serial.print(": ");

        if (WiFi.hostByName(
                SUPABASE_HOST,
                ipSupabase))
        {
            Serial.println(ipSupabase);
            return true;
        }

        Serial.println("fallido");
        delay(1000);
    }

    return false;
}

static bool conectarInternet()
{
    IPAddress dnsPrincipal(
        8, 8, 8, 8
    );

    IPAddress dnsSecundario(
        1, 1, 1, 1
    );

    WiFi.mode(WIFI_AP_STA);

    // Si ya está conectado, primero comprobar realmente el DNS.
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(
            "La ESP32 ya esta conectada al router"
        );

        Serial.print("IP local STA: ");
        Serial.println(WiFi.localIP());

        Serial.print("Gateway: ");
        Serial.println(WiFi.gatewayIP());

         Serial.print("DNS recibido: ");
        Serial.println(WiFi.dnsIP(0));

        forzarServidoresDNS();

        delay(500);

        if (probarDNSSupabase())
        {
            return true;
        }

        Serial.println(
            "La conexion existe, pero el DNS fallo."
        );

        Serial.println(
            "Reconectando con DNS manual..."
        );

        // Desconecta solo la interfaz STA.
        // El punto de acceso MonitorCardiaco se mantiene.
        WiFi.disconnect(false, false);
        delay(1000);
    }

    /*
     * Mantiene DHCP para IP, gateway y máscara,
     * pero establece servidores DNS manuales.
     */
    bool configuracionCorrecta =
        WiFi.config(
            INADDR_NONE,
            INADDR_NONE,
            INADDR_NONE,
            dnsPrincipal,
            dnsSecundario
        );

    if (!configuracionCorrecta)
    {
        Serial.println(
            "ADVERTENCIA: WiFi.config fallo"
        );
    }

    Serial.println();

    Serial.print("Conectando a Wi-Fi: ");
    Serial.println(WIFI_INTERNET_SSID);

    WiFi.begin(
        WIFI_INTERNET_SSID,
        WIFI_INTERNET_PASSWORD
    );

    unsigned long inicio =
        millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - inicio < 20000)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "ERROR: no se pudo conectar al router"
        );

        return false;
    }

    // Dar tiempo a DHCP y DNS para estabilizarse.
    delay(2000);

    Serial.println(
        "Wi-Fi con Internet conectado"
    );

    Serial.print("IP local STA: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("DNS recibido del router: ");
    Serial.println(WiFi.dnsIP(0));

    forzarServidoresDNS();

    delay(500);

    if (!probarDNSSupabase())
    {
        Serial.println(
            "ERROR: Supabase sigue sin resolverse por DNS"
        );

        return false;
    }

    Serial.println(
        "DNS de Supabase comprobado correctamente"
    );

    return true;
}

static String crearJSONSesion(const String& rutaPaciente, const String& rutaResumen, const String& nombreSesion)
{
    String estadoPaciente = Sincronizacion_leerValor(rutaPaciente, "Estado");
    if (estadoPaciente.isEmpty()) estadoPaciente = "pendiente";

    String cedulaPaciente = Sincronizacion_leerValor(rutaPaciente, "Cedula");
    String codigoSesionUnico = cedulaPaciente + "_" + nombreSesion;

    String json = "{";
    json += "\"dispositivo_codigo\":\"ESP32_001\",";

    json += "\"doctor\":{";
    json += "\"id\":\"" + escaparJSON(Sincronizacion_leerValor(rutaPaciente, "DoctorID")) + "\",";
    json += "\"nombre\":\"" + escaparJSON(Sincronizacion_leerValor(rutaPaciente, "DoctorNombre")) + "\"";
    json += "},";

    json += "\"paciente\":{";
    json += "\"cedula\":\"" + escaparJSON(cedulaPaciente) + "\",";
    json += "\"nombre\":\"" + escaparJSON(Sincronizacion_leerValor(rutaPaciente, "Nombre")) + "\",";
    json += "\"edad\":" + numeroJSON(Sincronizacion_leerValor(rutaPaciente, "Edad"), "0") + ",";
    json += "\"sexo\":\"" + escaparJSON(Sincronizacion_leerValor(rutaPaciente, "Sexo")) + "\",";
    json += "\"observaciones\":\"" + escaparJSON(Sincronizacion_leerValor(rutaPaciente, "Observaciones")) + "\",";
    json += "\"estado\":\"" + escaparJSON(estadoPaciente) + "\"";
    json += "},";

    json += "\"sesion\":{";
    json += "\"codigo_local\":\"" + escaparJSON(codigoSesionUnico) + "\",";
    json += "\"duracion_ms\":" + numeroJSON(Sincronizacion_leerValor(rutaResumen, "Duracion_ms")) + ",";
    json += "\"bpm_promedio\":" + numeroJSON(Sincronizacion_leerValor(rutaResumen, "BPM_promedio")) + ",";
    json += "\"bpm_minimo\":" + numeroJSON(Sincronizacion_leerValor(rutaResumen, "BPM_minimo")) + ",";
    json += "\"bpm_maximo\":" + numeroJSON(Sincronizacion_leerValor(rutaResumen, "BPM_maximo")) + ",";
    json += "\"spo2_promedio\":" + numeroJSON(Sincronizacion_leerValor(rutaResumen, "SpO2_promedio")) + ",";
    json += "\"spo2_minimo\":" + numeroJSON(Sincronizacion_leerValor(rutaResumen, "SpO2_minimo")) + ",";
    json += "\"spo2_maximo\":" + numeroJSON(Sincronizacion_leerValor(rutaResumen, "SpO2_maximo"));
    json += "}";
    json += "}";

    return json;
}

static bool enviarJSONSupabase(const String& payload)
{
    WiFiClientSecure cliente;
    cliente.setInsecure();

    HTTPClient http;

    if (!http.begin(cliente, SUPABASE_RPC_URL))
    {
        Serial.println("ERROR: no se pudo iniciar HTTPS");
        return false;
    }

    http.setTimeout(15000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

    String cuerpo = "{";
    cuerpo += "\"p_dispositivo_clave\":\"" + escaparJSON(DISPOSITIVO_CLAVE) + "\",";
    cuerpo += "\"p_payload\":" + payload;
    cuerpo += "}";

    Serial.println();
    Serial.println("Enviando registro a Supabase...");

    int codigoHTTP = http.POST(cuerpo);
    String respuesta = http.getString();

    Serial.print("Codigo HTTP: ");
    Serial.println(codigoHTTP);
    Serial.print("Respuesta: ");
    Serial.println(respuesta);

    http.end();

    bool codigoCorrecto = codigoHTTP >= 200 && codigoHTTP < 300;
    bool respuestaCorrecta =
        respuesta.indexOf("\"ok\":true") >= 0 ||
        respuesta.indexOf("\"ok\": true") >= 0;

    return codigoCorrecto && respuestaCorrecta;
}

static bool revisarPaciente(const String& cedula)
{
    String carpetaPaciente = "/PACIENTES/" + cedula;
    String rutaPaciente = carpetaPaciente + "/paciente.txt";

    if (!SD.exists(rutaPaciente)) return true;

    File carpeta = SD.open(carpetaPaciente);
    if (!carpeta || !carpeta.isDirectory()) return false;

    bool encontroSesion = false;
    bool resultadoPaciente = true;

    File elemento = carpeta.openNextFile();

    while (elemento)
    {
        if (elemento.isDirectory())
        {
            String nombreSesion = nombreBase(String(elemento.name()));
            String rutaResumen = carpetaPaciente + "/" + nombreSesion + "/resumen.txt";

            if (SD.exists(rutaResumen))
            {
                encontroSesion = true;

                String estadoSesion = Sincronizacion_leerValor(rutaResumen, "Sincronizacion");
                if (estadoSesion.isEmpty()) estadoSesion = "pendiente";

                if (estadoSesion == "pendiente")
                {
                    registrosPendientes++;
                    String json = crearJSONSesion(rutaPaciente, rutaResumen, nombreSesion);

                    Serial.println();
                    Serial.println("===== JSON A ENVIAR =====");
                    Serial.println(json);
                    Serial.println("===== FIN JSON =====");

                    if (enviarJSONSupabase(json))
                    {
                        if (escribirValorArchivo(rutaResumen, "Sincronizacion", "sincronizado"))
                            Serial.println("Registro sincronizado correctamente");
                        else
                        {
                            Serial.println("ADVERTENCIA: se envio, pero no se pudo marcar en la SD");
                            resultadoPaciente = false;
                        }
                    }
                    else
                    {
                        Serial.println("ERROR: Supabase no confirmo el registro");
                        Serial.println("La sesion permanece pendiente");
                        resultadoPaciente = false;
                    }
                }
            }
        }

        elemento.close();
        elemento = carpeta.openNextFile();
    }

    carpeta.close();

    if (!encontroSesion)
    {
        Serial.print("Paciente sin sesion finalizada: ");
        Serial.println(cedula);
    }

    return resultadoPaciente;
}

void Sincronizacion_begin()
{
    sincronizacionActiva = false;
    registrosPendientes = 0;
    Serial.println("Modulo de sincronizacion listo");
}

bool Sincronizacion_estaActiva()
{
    return sincronizacionActiva;
}

int Sincronizacion_getPendientes()
{
    return registrosPendientes;
}

bool Sincronizacion_ejecutar()
{
    if (sincronizacionActiva)
    {
        Serial.println("Ya existe una sincronizacion en curso");
        return false;
    }

    sincronizacionActiva = true;
    registrosPendientes = 0;

    if (!conectarInternet())
    {
        sincronizacionActiva = false;
        Serial.println("Los archivos permanecen pendientes");
        return false;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("INICIANDO SINCRONIZACION");
    Serial.println("================================");

    if (!MicroSD_disponible())
    {
        Serial.println("ERROR: MicroSD no disponible");
        sincronizacionActiva = false;
        return false;
    }

    if (!SD.exists("/PACIENTES"))
    {
        Serial.println("No existe la carpeta /PACIENTES");
        sincronizacionActiva = false;
        return true;
    }

    File carpetaPacientes = SD.open("/PACIENTES");
    if (!carpetaPacientes || !carpetaPacientes.isDirectory())
    {
        Serial.println("ERROR: no se pudo abrir /PACIENTES");
        sincronizacionActiva = false;
        return false;
    }

    bool resultadoGeneral = true;
    File paciente = carpetaPacientes.openNextFile();

    while (paciente)
    {
        if (paciente.isDirectory())
        {
            String cedula = nombreBase(String(paciente.name()));
            if (!revisarPaciente(cedula)) resultadoGeneral = false;
        }

        paciente.close();
        paciente = carpetaPacientes.openNextFile();
    }

    carpetaPacientes.close();

    Serial.println();
    Serial.print("Registros pendientes procesados: ");
    Serial.println(registrosPendientes);

    if (registrosPendientes == 0)
        Serial.println("No hay sesiones pendientes");

    Serial.println("================================");

    sincronizacionActiva = false;
    return resultadoGeneral;
}

bool Sincronizacion_probarLecturaSD()
{
    return Sincronizacion_ejecutar();
}