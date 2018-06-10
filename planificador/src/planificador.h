#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <commons/collections/list.h>
#include <commons/error.h>
#include <commons/string.h>
#include <biblio.h>

#include "adminESI.h"

typedef struct {
	id_t id_esi;
	enum tipoDeInstruccion tipo;
	uint8_t tamClave;
	char* clave;
	booleano resultado;
} consultaCoord;

enum comandos {pausar, continuar, bloquear, desbloquear, listar, kill, status, deadlock, salir};

typedef uint8_t instruccionesCoord;

void inicializacion(char*);
void bloquearClavesConfiguracion();
socket_t conectarConCoordinador();
int enviarEncabezado(socket_t, int);
int enviarIdESI(socket_t, int);
int enviarRespuestaConsultaCoord(socket_t, booleano);
consultaCoord* recibirConsultaCoord(socket_t);
socket_t crearServerESI();
int procesarConsultaCoord(ESI*, consultaCoord*);
pthread_t crearHiloTerminal ();
pthread_t crearHiloNuevasESI ();
pthread_t crearHiloEjecucion (ESI*);
pthread_t crearHiloCoordinador (socket_t);

enum comandos convertirComando(char *);
void salirConError(char *);
void liberarRecursos();

void terminal();
void escucharPorESI ();
void ejecucionDeESI (ESI*);
void comunicacionCoord(socket_t);

void comandoBloquear(char**);
void comandoDesbloquear(char**);
void comandoListar(char**);


#endif /* PLANIFICADOR_H_ */
