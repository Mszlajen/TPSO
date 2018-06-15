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
	enum resultadoEjecucion res;
	char* clave;
	char* valor;
	ESI_id idEsi;
} Esi;


typedef struct {
	socket_t socket;
	instancia_id idinstancia;
	char* nombre;
	Esi * esiTrabajando;
} Instancia;

#define IPEscucha "127.0.0.2"
#define Puerto "Puerto"

void esESIoInstancia (socket_t socketAceptado);
int esperarYaceptar(socket_t socketCoordinador,struct sockaddr_in* dir);
int validarPlanificador (socket_t socket);
void liberarRecursos();
void salirConError(char * error);
void inicializacion();
void registrarEsi(socket_t socket);
void registrarInstancia(socket_t socket);
bool idInstancia(Instancia * instancia);
void tratarGetInstancia(Instancia * instancia);
void tratarGetEsi(Esi * esi);
void tratarSetStoreEsi(Esi * esi);
void tratarSetInstancia(Instancia * instancia);
void tratarStoreInstancia(Instancia * instancia);
int consultarPorClaveTomada(Esi esi);
void hiloInstancia (Instancia * instancia);
void hiloEsi (Esi * esi);
void hiloPlanificador (socket_t socket);
void recibirConexiones();
Instancia * algoritmoUsado(void);
Instancia * algoritmoEquitativeLoad(void);
int crearConfiguracion(char * archivoConfig);
int buscarClaves(char * clave);
int buscarInstanciaPorId(Instancia * instancia);
void setearEsiActual(Esi esi);
void recibirResultadoInstancia(Instancia * instancia);
void enviarResultadoEsi(Esi * esi);







#endif /* SRC_COORDINADOR_H_ */
