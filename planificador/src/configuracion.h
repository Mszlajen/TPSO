#ifndef CONFIGURACION_H_
#define CONFIGURACION_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/string.h>

#define Ip "IP"
#define Puerto "Puerto"
#define Algoritmo "Algoritmo"
#define Alfa "Alfa"
#define Estimacion "Estimacion"
#define IPCoord "IPCoord"
#define PuertoCoord "PuertoCoord"
#define ClavesBloq "ClavesBloqueadas"

enum t_algoritmo {sjf, srt, hrrn};

int crearConfiguracion(char*); //Devuelve 0 en exito o 1 en fallo
void eliminarConfiguracion();
char* obtenerIP();
char* obtenerPuerto();
char* obtenerDireccionCoordinador();
char* obtenerPuertoCoordinador();
float obtenerAlfa();
int obtenerEstimacionInicial();
enum t_algoritmo obtenerAlgoritmo();
char* obtenerBloqueosConfiguracion();

#endif /* CONFIGURACION_H_ */
