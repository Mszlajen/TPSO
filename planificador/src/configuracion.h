#ifndef CONFIGURACION_H_
#define CONFIGURACION_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/string.h>

#define archivoConfig "planificador.config"
#define Puerto "Puerto"
#define Algoritmo "Algoritmo"
#define Alfa "Alfa"
#define Estimacion "Estimacion"
#define IPCoord "IPCoord"
#define PuertoCoord "PuertoCoord"
#define ClavesBloq "ClavesBloq"

enum t_algoritmo {sjf, srt, hrrn};

int crearConfiguracion(); //Devuelve 0 en exito o 1 en fallo
void eliminarConfiguracion();
char* obtenerPuerto();
char* obtenerDireccionCoordinador();
char* obtenerPuertoCoordinador();
float obtenerAlfa();
int obtenerEstimacionInicial();
enum t_algoritmo obtenerAlgoritmo();

#endif /* CONFIGURACION_H_ */
