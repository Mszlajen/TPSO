#ifndef CONFIGURACION_H_
#define CONFIGURACION_H_

#include <stdio.h>
#include <commons/config.h>
#include <commons/string.h>

#define nombre "nombreInstancia"
#define IPCoord "IPCoordinador"
#define puertoCoord "puertoCoordinador"
#define puntoMontaje "puntoDeMontaje"
#define tiempoDump "tiempoIntervaloDump"
#define algoReempl "algoritmoDeReemplazo"

enum algoritmoReemplazo {BSU, LRU, Circular};

int crearConfiguracion(char*);

char* obtenerNombreInstancia();
char* obtenerIPCordinador();
char* obtenerPuertoCoordinador();
char* obtenerPuntoDeMontaje();
int obtenerIntervaloDeDump();
enum algoritmoReemplazo obtenerAlgoritmoReemplazo();
// Devuelve un nuevo string independiente de la configuracion.
char* obtenerDireccionArchivoID();
// Devuelve un nuevo string independiente de la configuracion.
char* obtenerDireccionArchivoMontaje(char*);

void destruirConfiguracion();
#endif /* CONFIGURACION_H_ */
