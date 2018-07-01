#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>
#include <readline/readline.h>
#include <commons/collections/list.h>
#include <commons/error.h>
#include <commons/string.h>
#include <biblio.h>

#include "adminESI.h"

typedef struct {
	ESI_id id_esi;
	enum tipoDeInstruccion tipo;
	tamClave_t tamClave;
	char* clave;
	booleano resultado;
} consultaCoord;

typedef struct {
	tamClave_t tamClave;
	char* clave;
	instancia_id id;
	tamNombreInstancia_t tamNombre;
	char* nombre;
	enum estadoClave estado;
	tamValor_t tamValor;
	char* valor;
} consultaStatus;

enum comandos {pausar, continuar, bloquear, desbloquear, listar, kill, status, deadlock, salir};

void inicializacion(char*);
void ejecucionDeESI (ESI*);

void bloquearClavesConfiguracion();
socket_t conectarConCoordinador();
int enviarEncabezado(socket_t, int);
int enviarIdESI(socket_t, int);
int enviarRespuestaConsultaCoord(socket_t, booleano);
consultaCoord* recibirConsultaCoord(socket_t);
void recibirRespuestaCoordinador(socket_t, consultaStatus*);
void recibirRespuestaStatus(socket_t, consultaStatus*);
socket_t crearServerESI();

pthread_t crearHiloTerminal ();
pthread_t crearHiloNuevasESI ();
pthread_t crearFinEjecucion (ESI*);
pthread_t crearHiloCoordinador ();

enum comandos convertirComando(char *);
void salirConError(char *);
void liberarRecursos();

void terminal();
void escucharPorESI ();
void comunicacionCoord(socket_t);
void escucharPorFinESI();

void comandoBloquear(char*, char*);
void comandoDesbloquear(char*);
void comandoListar(char*);
void comandoKill(char*);
void comandoStatus(char*);
void comandoDeadlock();

void finalizarESIBien(ESI* esi);
void finalizarESIMal(ESI* esi);

int max (int, int);
void terminarEjecucionESI(ESI*);
#endif /* PLANIFICADOR_H_ */
