#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <readline/readline.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/error.h>
#include <biblio.h>

enum t_algoritmo {sjf, srt, hrrn};

typedef struct {
	int id;
	int socket;
	int estimacion;
	t_list *recursos;
}ESI;


void inicializacion(char*);
int crearSocketParaESI ();
int conectarConCoordinador();
void enviarHandshake(int);
pthread_t crearHiloTerminal ();
pthread_t crearHiloNuevasESI ();
void escucharPorESI ();
ESI* crearESI (int);

void terminal();

#endif /* PLANIFICADOR_H_ */
