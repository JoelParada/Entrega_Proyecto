#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ECG.h"
#include "Config.h"
#include "OLED.h"
#include "MAX30102.h"
#include "ServidorWeb.h"
#include "MicroSD.h"
#include "Sesiones.h"
#include "SensorMAX30102.h"
//#include "HeartRateMAX30102.h"
#include "ModoAhorro.h"
#include "Sincronizacion.h"
#include "Doctores.h"
#include "RTC.h"
// ====================================================================
// CONFIGURACIÓN DE PANTALLA OLED Y PINES
// ====================================================================


// ====================================================================
// VARIABLES DE ESTADO CLÍNICO Y SISTEMA
// ====================================================================
int spo2Calculado = -1;
float tempCalculada = 36.5;
int bpmMAX30102 = 0;

int candidatoBPM = 0;
byte confirmacionesBPM = 0;

const byte CONFIRMACIONES_NECESARIAS = 3;
const int TOLERANCIA_BPM = 10;

int bpmInstantaneoMAX = -1;

unsigned long tiempoUltimoBPMValido = 0;
unsigned long inicioBPMCritico = 0;

bool alarmaBPMConfirmada = false;

unsigned long tiempoUltimaAlarma = 0;
bool estadoAlarma = false;

const unsigned long TIEMPO_ANTIRREBOTE = 400;

// Variables para el control de la interfaz gráfica (OLED)
int xOLED = 0;
int lastYOLED = 40;
unsigned long tiempoAnterior = 0;
int estadoPantalla = 1;      // 0: BPM, 1: Gráfica, 2: SpO2/Temp, 3: Resumen
bool repintarCabecera = true;
unsigned long tiempoUltimoBoton = 0;


void setup()
{
    ECG_begin();
    Serial.begin(115200);

    // Configuración de pines
    pinMode(pinLED, OUTPUT);
    digitalWrite(pinLED, LOW);

    pinMode(pinBuzzer, OUTPUT);
    digitalWrite(pinBuzzer, LOW);

    // Pines AD8232
    pinMode(pinLOPlus, INPUT);
    pinMode(pinLOMinus, INPUT);

    pinMode(btnAnt, INPUT_PULLUP);
    pinMode(btnSig, INPUT_PULLUP);

    analogReadResolution(12);

    // ============================================================
    // DISPOSITIVOS I2C
    // ============================================================

    OLED_begin();

    // RTC DS3231
    if (!RTC_begin())
    {
        Serial.println("ERROR: RTC DS3231 no encontrado");
    }

    if (!MAX30102_begin())
    {
        Serial.println("ERROR: MAX30102 no encontrado");
        while (1);
    }

    Serial.println("MAX30102 listo");

    // ============================================================
    // MICROSD
    // ============================================================

    MicroSD_begin();

    // ============================================================
    // DOCTORES
    // ============================================================

    if (!Doctor_begin())
    {
        Serial.println(
            "ADVERTENCIA: modulo Doctores no disponible"
        );
    }

    // ============================================================
    // SERVIDOR WEB
    // ============================================================

    WebServer_begin();

    tiempoAnterior = millis();

    // ============================================================
    // MODO AHORRO
    // ============================================================

    ModoAhorro_begin();

    // ============================================================
    // SINCRONIZACION
    // ============================================================

    Sincronizacion_begin();
    Sincronizacion_probarLecturaSD();
}

void loop() {
  //Serial.println("LOOP");
    

  unsigned long tiempoActualLoop = millis();

// ================================================================
// BOTON FISICO DE SINCRONIZACION
// btnSig = GPIO 14
// ================================================================

static bool estadoAnteriorBtnSig = HIGH;
static unsigned long ultimoPulsoSincronizacion = 0;

bool estadoActualBtnSig =
    digitalRead(btnSig);

if (estadoAnteriorBtnSig == HIGH &&
    estadoActualBtnSig == LOW &&
    tiempoActualLoop - ultimoPulsoSincronizacion >= 500)
{
    ultimoPulsoSincronizacion =
        tiempoActualLoop;

    ModoAhorro_registrarActividad();

    Serial.println();
    Serial.println(
        "Boton de sincronizacion presionado"
    );

    if (!Sincronizacion_estaActiva())
    {
        bool resultado =
            Sincronizacion_ejecutar();

        if (resultado)
        {
            Serial.print(
                "Pendientes preparados: "
            );

            Serial.println(
                Sincronizacion_getPendientes()
            );
        }
        else
        {
            Serial.println(
                "ERROR al revisar los pendientes"
            );
        }
    }
}

estadoAnteriorBtnSig = estadoActualBtnSig;

  static unsigned long ultimoBotonAhorro = 0;

if (digitalRead(btnAnt) == LOW &&
    tiempoActualLoop - ultimoBotonAhorro >= 400)
{
    ultimoBotonAhorro = tiempoActualLoop;

    ModoAhorro_registrarActividad();

    while (digitalRead(btnAnt) == LOW)
    {
        delay(10);
    }
}


ModoAhorro_update();

if (ModoAhorro_estaActivo())
{
    digitalWrite(pinLED, LOW);
    digitalWrite(pinBuzzer, LOW);

    WebServer_update(
    -1,
    -1,
    tempCalculada
);

    delay(10);
    return;
}

// ================================================================
// ACTUALIZAR MAX30102
// ================================================================

MAX30102_update();

int lecturaBPM = MAX30102_getHeartRate();

bpmInstantaneoMAX = lecturaBPM;

if (lecturaBPM >= 25 && lecturaBPM <= 220)
{
    tiempoUltimoBPMValido = tiempoActualLoop;

    // Primera lectura candidata
    if (candidatoBPM == 0)
    {
        candidatoBPM = lecturaBPM;
        confirmacionesBPM = 1;
    }
    // La nueva lectura se parece a la candidata
    else if (abs(lecturaBPM - candidatoBPM) <= TOLERANCIA_BPM)
    {
        candidatoBPM =
            static_cast<int>(
                candidatoBPM * 0.70f +
                lecturaBPM * 0.30f
            );

        if (confirmacionesBPM < CONFIRMACIONES_NECESARIAS)
        {
            confirmacionesBPM++;
        }
    }
    // Lectura muy diferente: iniciar una nueva candidata
    else
    {
        candidatoBPM = lecturaBPM;
        confirmacionesBPM = 1;
    }

    // Solo aceptar el valor después de varias confirmaciones
    if (confirmacionesBPM >= CONFIRMACIONES_NECESARIAS)
    {
        if (bpmMAX30102 == 0)
        {
            bpmMAX30102 = candidatoBPM;
        }
        else
        {
            bpmMAX30102 =
                static_cast<int>(
                    bpmMAX30102 * 0.80f +
                    candidatoBPM * 0.20f
                );
        }
    }
}

if (!MAX30102_hayDedo())
{
    bpmMAX30102 = 0;
    bpmInstantaneoMAX = -1;

    candidatoBPM = 0;
    confirmacionesBPM = 0;

    tiempoUltimoBPMValido = 0;
    inicioBPMCritico = 0;
    alarmaBPMConfirmada = false;
}

// Mostrar datos solo una vez por segundo
static unsigned long ultimoDebugMAX = 0;

if (tiempoActualLoop - ultimoDebugMAX >= 1000)
{
    ultimoDebugMAX = tiempoActualLoop;

    //Serial.print("BPM MAX30102: ");
    //Serial.print(MAX30102_getHeartRate());

    //Serial.print(" | SpO2: ");
    //Serial.println(MAX30102_getSpO2());
}
//Serial.print("IR: ");
//Serial.println(sensor.getIR());
  // ====================================================================
  // TAREA 1: NAVEGACIÓN (Máquina de estados con Antirrebote)
  // ====================================================================
  // Se evalúa cada 250ms para evitar múltiples lecturas por un solo clic mecánico
  if (tiempoActualLoop - tiempoUltimoBoton > 250) {
    
  }

  // ====================================================================
  // TAREA 2: ADQUISICIÓN Y PROCESAMIENTO ECG (Ejecución estricta cada 10ms)
  // ====================================================================
  if (tiempoActualLoop - tiempoAnterior >= 10) {
    tiempoAnterior += 10;

    ECG_update();

    static unsigned long ultimoTestECG = 0;

if (millis() - ultimoTestECG >= 200)
{
    ultimoTestECG = millis();

    int crudo = analogRead(pinECG);

    Serial.print("CRUDO=");
    Serial.print(crudo);

    Serial.print(" | ECG=");
    Serial.print(ECG_getValue());

    Serial.print(" | LO+=");
    Serial.print(digitalRead(pinLOPlus));

    Serial.print(" | LO-=");
    Serial.println(digitalRead(pinLOMinus));
}

int ecgValue = ECG_getValue();

WebServer_agregarMuestraECG(ecgValue);


      // Leer SpO2 solamente si el dato es válido
      int lectura = MAX30102_getSpO2();

//Serial.print("SpO2 calculado: ");
//Serial.println(lectura);

if (lectura >= 70 && lectura <= 100)
{
    spo2Calculado = lectura;
}
else
{
    spo2Calculado = -1;
}

//Serial.print(" | Enviado a web: ");
//Serial.println(spo2Calculado);  //RECORDAR BORRAR
//Serial.print("BPM MAX30102: ");
//Serial.println(HeartRate_getBPM());

      // Temperatura (por ahora sigue siendo simulada)
      tempCalculada = map(analogRead(potTemp), 0, 4095, 340, 400) / 10.0;

      
      /*
      bool electrodoDesconectado =
        digitalRead(pinLOPlus) ||
        digitalRead(pinLOMinus);

      if (electrodoDesconectado)
        {
          bpmCalculado = 0;

          digitalWrite(pinLED, LOW);
          digitalWrite(pinBuzzer, LOW);

          Serial.println("Electrodos desconectados");
          return;
        }
        */
      //Serial.print("LO+: ");
//Serial.print(digitalRead(pinLOPlus)); DESMARCAR PARA REVISAR
//Serial.print("  LO-: ");
//Serial.println(digitalRead(pinLOMinus));
    
    bool bpmFueraDeRango =
    bpmInstantaneoMAX >= 25 &&
    (bpmInstantaneoMAX < 50 ||
     bpmInstantaneoMAX > 120);

bool sinPulsoValido =
    MAX30102_hayDedo() &&
    tiempoUltimoBPMValido > 0 &&
    tiempoActualLoop - tiempoUltimoBPMValido >= 5000;

if (bpmFueraDeRango || sinPulsoValido)
{
    if (inicioBPMCritico == 0)
    {
        inicioBPMCritico = tiempoActualLoop;
    }

    // La condición debe mantenerse 3 segundos.
    if (tiempoActualLoop - inicioBPMCritico >= 5000)
    {
        alarmaBPMConfirmada = true;
    }
}
else
{
    inicioBPMCritico = 0;
    alarmaBPMConfirmada = false;
}
    
bool alarmaBradicardia =
    alarmaBPMConfirmada &&
    bpmMAX30102 > 0 &&
    bpmMAX30102 < 50;

bool alarmaTaquicardia =
    alarmaBPMConfirmada &&
    bpmMAX30102 > 120;

bool alarmaHipoxia =
    spo2Calculado > 0 &&
    spo2Calculado < 90;

bool alarmaFiebre =
    tempCalculada > 38.5;

bool alarmaHipotermia =
    tempCalculada < 35.5;

bool alarmaSinPulso =
    MAX30102_hayDedo() &&
    bpmMAX30102 == 0;

// Lógica consolidada de alarmas (Taquicardia, Bradicardia, Hipoxia, Fiebre, Hipotermia)
    bool alarmaCritica =
    alarmaBradicardia ||
    alarmaTaquicardia ||
    alarmaHipoxia ||
    alarmaFiebre ||
    alarmaHipotermia ||
    alarmaSinPulso;

    if (alarmaCritica) {
      if (tiempoActualLoop - tiempoUltimaAlarma > 150) { 
        estadoAlarma = !estadoAlarma;
        digitalWrite(pinBuzzer, estadoAlarma);
        tiempoUltimaAlarma = tiempoActualLoop;
      }
    } else {
      digitalWrite(pinBuzzer, LOW); // Bip normal sincronizado con latido
    }

    // Dibujo en tiempo real de la onda (Exclusivo del estado 1)
    if (estadoPantalla == 1) {
      if (repintarCabecera) {
        display.fillRect(0, 0, 128, 16, SSD1306_BLACK);
        repintarCabecera = false;
      }

      int yOLED = map(ecgValue, 1500, 4000, 63, 16);
      yOLED = constrain(yOLED, 16, 63);
      int nextX = xOLED + 1;
      
      // Reinicio de la pantalla al llegar al borde derecho
      if (nextX >= SCREEN_WIDTH) {
        display.fillRect(0, 16, 128, 48, SSD1306_BLACK);
        xOLED = 0; nextX = 0; lastYOLED = yOLED;
      }
      
      display.drawLine(xOLED, lastYOLED, nextX, yOLED, SSD1306_WHITE);
      display.drawPixel(nextX, yOLED, SSD1306_WHITE);
      xOLED = nextX; lastYOLED = yOLED;

      // Refresco parcial de la cabecera para no afectar el rendimiento
      static uint8_t contadorRefresco = 0;
if (++contadorRefresco >= 4) {
    display.fillRect(72, 0, 56, 10, SSD1306_BLACK);
    display.setCursor(72, 0);
    display.print("BPM:");
    display.print(
    bpmMAX30102 == 0 ? "--" : String(bpmMAX30102)
    );
    display.display();
    contadorRefresco = 0;
}
    }
  } 

  // ====================================================================
  // TAREA 3: ACTUALIZACIÓN DE INTERFACES SECUNDARIAS (Cada 200ms)
  // ====================================================================
  static unsigned long lastUpdateSimuladores = 0;
  if (tiempoActualLoop - lastUpdateSimuladores > 200) {
    
    if (estadoPantalla != 1) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      switch(estadoPantalla) {
        case 0: { // Modo: Visualización ampliada de BPM
          display.setCursor(0, 20); display.setTextSize(2);
          display.print("BPM: "); 
          display.print(bpmMAX30102);
          break;
        }
        case 2: { // Modo: Sensores ambientales y de saturación
          display.setCursor(0, 10); display.print("SpO2: "); display.print(spo2Calculado); display.print("%");
          display.setCursor(0, 30); display.print("Temp: "); display.print(tempCalculada); display.print(" C");
          if(spo2Calculado < 90) { display.setCursor(0, 50); display.print("! HIPOXIA !"); }
          else if(tempCalculada > 38.5) { display.setCursor(0, 50); display.print("! FIEBRE !"); }
          break;
        }
        case 3: { // Modo: Dashboard general clínico
          display.setCursor(0, 0);  display.print("BPM:  "); display.print(bpmMAX30102);
          display.setCursor(0, 20); display.print("SpO2: "); display.print(spo2Calculado); display.print("%");
          display.setCursor(0, 40); display.print("Temp: "); display.print(tempCalculada); display.print(" C");
          break;
        }
      }
      display.display();
    }
    lastUpdateSimuladores = tiempoActualLoop;
  }
  Sesion_actualizar(
    bpmMAX30102,
    spo2Calculado,
    ECG_getValue()
);
  WebServer_update(
    bpmMAX30102,
    spo2Calculado,
    tempCalculada
);

}