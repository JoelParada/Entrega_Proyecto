#ifndef PACIENTES_H
#define PACIENTES_H

#include <Arduino.h>

// Versión compatible con código antiguo.
bool Paciente_guardar(
    const String& nombre,
    const String& cedula,
    int edad,
    const String& sexo,
    const String& observaciones
);

// Guarda al paciente asociado a un doctor.
bool Paciente_guardar(
    const String& nombre,
    const String& cedula,
    int edad,
    const String& sexo,
    const String& observaciones,
    const String& doctorId,
    const String& doctorNombre
);

// Devuelve en JSON los pacientes pertenecientes al doctor.
String Paciente_listarPorDoctorJSON(
    const String& doctorId
);

// Cambia el estado del paciente a revisado.
// Solo permite modificarlo si pertenece al doctor indicado.
bool Paciente_marcarRevisado(
    const String& cedula,
    const String& doctorId
);

#endif