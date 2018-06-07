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
#include <commons/log.c>

typedef struct {
	socket_t socket;
	enum instruccion instr;
	char* clave;
	char* valor;
} Esi;


typedef struct {
	socket_t socket;
	int idinstancia;
	char* nombre;
	Esi * esiTrabajando;
} Instancia;

#define IPEscucha "127.0.0.2"
#define archivoConfig "coordinador.config"
#define Puerto "Puerto"

void hiloDeInstancia();
void esESIoInstancia (socket_t socketAceptado,struct sockaddr_in dir);
int esperarYaceptar(socket_t socketCoordinador,struct sockaddr_in* dir);
int validarPlanificador (socket_t socket);
void liberarRecursos();
void salirConError(char * error);
void inicializacion();
void atenderEsi(socket_t socket);
void registrarEsi(socket_t socket);
void registrarInstancia(socket_t socket);
void reconectarInstancia(socket_t socket);
bool idInstancia(Instancia instancia);
void escucharPorAcciones ();
void tratarGet(Esi * esi);
void tratarSet(Esi * esi);
void tratarStore(Esi * esi);
int consultarPorClaveTomada(Esi esi);
void setearReadfdsInstancia (Instancia instancia);
void setearReadfdsEsi (Esi esi);
void escucharReadfdsInstancia (Instancia  instancia);
void escucharReadfdsEsi (Esi esi);
void recibirConexiones();
Instancia * algoritmoUsado();
Instancia * algoritmoEquitativeLoad();







#endif /* SRC_COORDINADOR_H_ */
