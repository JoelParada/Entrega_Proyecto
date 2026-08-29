#ifndef CONFIG_H
#define CONFIG_H
//MECOMIUNSASALCHIPAPA
//================ OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

//================ AD8232 ===============
const int pinECG = 35;
const int pinLOPlus = 32;
const int pinLOMinus = 33;

//================ Indicadores ==========
const int pinLED = 2;
const int pinBuzzer = 4;

//================ Temporales ===========
const int potSpo2 = 25;
const int potTemp = 34;

//================ Botones ==============
const int btnAnt = 27;
const int btnSig = 14;

//================ MICRO SD =============
const int pinSD_CS = 5;

//================ BOTÓN REC =============
const int btnREC = 13;

#endif