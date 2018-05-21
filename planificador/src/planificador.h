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
#include <parsi/parser.h>

#define IPConexion "127.0.0.2"

typedef struct {
	id_t esi_id;
	t_esi_operacion instruccion;
} consultaCoord;

enum comandos {pausa, bloquear, desbloquear, listar, kill, status, deadlock, salir};

void inicializacion();
void conectarConCoordinador();
int enviarEncabezado(socket_t, int);
int enviarIdESI(socket_t, int);
int enviarRespuestaConsultaCoord(socket_t, int);
consultaCoord* recibirConsultaCoord();
void crearServerESI();
int procesarConsultaCoord(t_esi_operacion*);
pthread_t crearHiloTerminal ();
pthread_t crearHiloNuevasESI ();
pthread_t crearHiloEjecucion ();

enum comandos convertirComando(char *);
void salirConError(char *);
void liberarRecursos();

void terminal();
void escucharPorESI ();
void ejecucionDeESI ();



#endif /* PLANIFICADOR_H_ */
