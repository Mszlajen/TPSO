#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <readline/readline.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/error.h>
#include <commons/string.h>
#include <biblio.h>

enum t_algoritmo {sjf, srt, hrrn};
enum comandos {pausa, bloquear, desbloquear, listar, kill, status, deadlock};

typedef struct {
	int id;
	int socket;
	int estimacion;
	t_list *recursos;
}ESI;


void inicializacion();
socket_t conectarConCoordinador();
void enviarHandshake(socket_t);
pthread_t crearHiloTerminal ();
pthread_t crearHiloNuevasESI ();

ESI* crearESI (int);
enum comandos convertirComando(char *);

void terminal();
void escucharPorESI ();

#endif /* PLANIFICADOR_H_ */
