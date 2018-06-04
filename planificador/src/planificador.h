#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <readline/readline.h>
#include <commons/collections/list.h>
#include <commons/error.h>
#include <commons/string.h>
#include <biblio.h>

#include "adminESI.h"

typedef struct {
	enum tipoDeInstruccion tipo;
	uint8_t tamClave;
	char* clave;
} consultaCoord;

typedef uint8_t resultado_t;

enum comandos {pausar, continuar, bloquear, desbloquear, listar, kill, status, deadlock, salir};

void inicializacion(char*);
void bloquearClavesConfiguracion();
void conectarConCoordinador();
int enviarEncabezado(socket_t, int);
int enviarIdESI(socket_t, int);
int enviarRespuestaConsultaCoord(socket_t, uint8_t);
consultaCoord* recibirConsultaCoord();
void crearServerESI();
int procesarConsultaCoord(ESI*, consultaCoord*);
pthread_t crearHiloTerminal ();
pthread_t crearHiloNuevasESI ();
pthread_t crearHiloEjecucion ();

enum comandos convertirComando(char *);
void salirConError(char *);
void liberarRecursos();

void terminal();
void escucharPorESI ();
void ejecucionDeESI ();

void comandoBloquear(char**);
void comandoDesbloquear(char**);
void comandoListar(char**);


#endif /* PLANIFICADOR_H_ */
