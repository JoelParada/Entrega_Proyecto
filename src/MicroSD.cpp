/*#include "MicroSD.h"
#include "Config.h"
#include <SPI.h>
#include <SD.h>

static bool disponible=false;

bool MicroSD_begin(){
    SPI.begin(18,19,23,pinSD_CS);
    if(!SD.begin(pinSD_CS,SPI,1000000)){
        disponible=false;
        Serial.println("ADVERTENCIA: MicroSD no detectada");
        return false;
    }
    if(SD.cardType()==CARD_NONE){
        disponible=false;
        Serial.println("ADVERTENCIA: No hay tarjeta insertada");
        return false;
    }
    disponible=true;
    Serial.println("MicroSD conectada correctamente");
    return true;
}

bool MicroSD_disponible(){ return disponible; }*/

#include "MicroSD.h"
#include "Config.h"
#include <SPI.h>
#include <SD.h>

static bool disponible=false;

bool MicroSD_begin(){
    SPI.begin(18,19,23,pinSD_CS);
    if(!SD.begin(pinSD_CS,SPI,1000000)){
        disponible=false;
        Serial.println("ADVERTENCIA: MicroSD no detectada");
        return false;
    }
    if(SD.cardType()==CARD_NONE){
        disponible=false;
        Serial.println("ADVERTENCIA: No hay tarjeta insertada");
        return false;
    }
    disponible=true;
    Serial.println("MicroSD conectada correctamente");
    return true;
}

bool MicroSD_disponible(){ return disponible; }