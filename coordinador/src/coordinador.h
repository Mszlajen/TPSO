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

typedef struct {
	int socket;
	int idinstancia;
	char * nombre;
} Instancia;

typedef struct {
	int socket;
	pthread_t hiloEsi;
} Esi;

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
void atenderEsi(int socket);
void registrarEsi(int socket);
void registrarInstancia(int socket,int* mensaje);




#endif /* SRC_COORDINADOR_H_ */
