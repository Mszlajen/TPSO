#ifndef SRC_COORDINADOR_H_
#define SRC_COORDINADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <biblio.h>
#include <string.h>
#include <commons/config.h>
#include <commons/collections/list.h>


#define IPEscucha "127.0.0.2"
#define archivoConfig "coordinador.config"
#define Puerto "Puerto"

void hiloDeInstancia();
void esESIoInstancia (int socketAceptado,struct sockaddr_in dir);
int esperarYaceptar(int socketCoordinador, int colaMax,struct sockaddr_in* dir);
int validarPlanificador (int socket);
void liberarRecursos();
void salirConError(char * error);
void inicializacion();



#endif /* SRC_COORDINADOR_H_ */
