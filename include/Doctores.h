#ifndef DOCTORES_H
#define DOCTORES_H

#include <Arduino.h>

// Inicializa la carpeta y el archivo de doctores.
bool Doctor_begin();

// Registra un doctor nuevo.
// Devuelve false si los datos son inválidos,
// el usuario ya existe o falla la MicroSD.
bool Doctor_registrar(
    const String& nombre,
    const String& usuario,
    const String& contrasena
);

// Inicia sesión verificando las credenciales
// guardadas en la MicroSD.
bool Doctor_iniciarSesion(
    const String& usuario,
    const String& contrasena
);

// Cierra la sesión actual.
void Doctor_cerrarSesion();

// Estado de autenticación.
bool Doctor_autenticado();

// Datos del doctor autenticado.
String Doctor_getId();
String Doctor_getNombre();
String Doctor_getUsuario();

// Verifica si un nombre de usuario ya existe.
bool Doctor_usuarioExiste(const String& usuario);

#endif