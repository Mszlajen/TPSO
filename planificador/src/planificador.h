#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
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

enum comandos {pausar, continuar, bloquear, desbloquear, listar, kill, status, deadlock, salir};

void inicializacion(char*);
void ejecucionDeESI (ESI*);

void bloquearClavesConfiguracion();
socket_t conectarConCoordinador();
int enviarEncabezado(socket_t, int);
int enviarIdESI(socket_t, int);
int enviarRespuestaConsultaCoord(socket_t, booleano);
consultaCoord* recibirConsultaCoord(socket_t);
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

int max (int, int);
void terminarEjecucionESI(ESI*);
#endif /* PLANIFICADOR_H_ */
