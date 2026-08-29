#ifndef RTC_MODULO_H
#define RTC_MODULO_H

#include <Arduino.h>

bool RTC_begin();
bool RTC_disponible();

String RTC_getFecha();
String RTC_getHora();
String RTC_getFechaHora();

String RTC_getFechaArchivo();
String RTC_getHoraArchivo();

int RTC_getDia();
int RTC_getMes();
int RTC_getAnio();

int RTC_getHoraNumero();
int RTC_getMinuto();
int RTC_getSegundo();

#endif