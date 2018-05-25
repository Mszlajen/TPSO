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
	socket_t socket;
	int idinstancia;
	char nombre[10];
} Instancia;

typedef struct {
	socket_t socket;
	enum instruccion instr;
	void* clave;
	void* valor;
	// como estan definidos los resultados de ejecucion?
} Esi;

#define IPEscucha "127.0.0.2"
#define archivoConfig "coordinador.config"
#define Puerto "Puerto"

void hiloDeInstancia();
void esESIoInstancia (socket_t socketAceptado,struct sockaddr_in dir);
int esperarYaceptar(socket_t socketCoordinador, int colaMax,struct sockaddr_in* dir);
int validarPlanificador (socket_t socket);
void liberarRecursos();
void salirConError(char * error);
void inicializacion();
void atenderEsi(socket_t socket);
void registrarEsi(socket_t socket);
void registrarInstancia(socket_t socket);




#endif /* SRC_COORDINADOR_H_ */
