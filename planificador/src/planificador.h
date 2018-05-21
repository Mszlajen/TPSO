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

#define IPConexion "127.0.0.2"

enum comandos {pausa, bloquear, desbloquear, listar, kill, status, deadlock, salir};

void inicializacion();
void conectarConCoordinador();
void enviarHandshake(socket_t);
void crearServerESI();
pthread_t crearHiloTerminal ();
pthread_t crearHiloNuevasESI ();

enum comandos convertirComando(char *);
void salirConError(char *);
void liberarRecursos();

void terminal();
void escucharPorESI ();



#endif /* PLANIFICADOR_H_ */
