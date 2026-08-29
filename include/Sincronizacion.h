#ifndef SINCRONIZACION_H
#define SINCRONIZACION_H

#include <Arduino.h>

// Inicializa el módulo.
void Sincronizacion_begin();

// Ejecuta una revisión completa de la MicroSD.
// Por ahora prepara e imprime los JSON pendientes.
// Más adelante esta misma función enviará los JSON a Supabase.
bool Sincronizacion_ejecutar();

// Indica si existe una sincronización en curso.
bool Sincronizacion_estaActiva();

// Devuelve cuántos registros pendientes encontró
// en la última ejecución.
int Sincronizacion_getPendientes();

// Utilidad para leer Clave=Valor desde un archivo.
String Sincronizacion_leerValor(
    const String& rutaArchivo,
    const String& clave
);

// Prueba y muestra el contenido útil de la MicroSD.
bool Sincronizacion_probarLecturaSD();

#endif