#ifndef SRC_COORDINADOR_H_
#define SRC_COORDINADOR_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <biblio.h>
#include <string.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/log.c>

typedef struct {
	instancia_id id;
	char* nombre;
	booleano conectada;
	t_list *claves;
	cantEntradas_t entradasLibres;
	sem_t sIniciarEjecucion;
	sem_t sFinCompactacion;
	socket_t socket;
	pthread_t hilo;
} instancia_t;

typedef struct {
	tamClave_t tamClave;
	char *clave;
	tamValor_t tamValor;
	char *valor;
	ESI_id id;
	enum instruccion instr;
	booleano validez;
	enum resultadoEjecucion result;
} operacion_t;

typedef struct {
	tamClave_t tamClave;
	char *clave;
	tamValor_t tamValor;
	char *valor;
	enum estadoClave estado;
	booleano atender;
	sem_t atendida;
} consultaStatus_t;

#define IPEscucha "IP"
#define Puerto "Puerto"

void inicializacion (char * dirConfiguracion);
socket_t conectarConPlanificador(socket_t socketEscucha);
void recibirNuevasConexiones(socket_t socketEscuchar);
void registrarInstancia(socket_t conexion);
void registrarESI(socket_t conexion);

void hiloESI (socket_t *socketESI);
void hiloInstancia(instancia_t *instancia);
void hiloPlanificador(socket_t *socketPlanificador);
void hiloStatus(socket_t *socketStatus);
void hiloCompactacion(instancia_t *llamadora);

void ejecutarGET();
void ejecutarSET();
void ejecutarSTORE();
void ejecutarCreate();

instancia_t *correrAlgoritmo(int *puntero);
instancia_t *algoritmoEquitativeLoad(int *puntero);
instancia_t *algoritmoLeastSpaceUsed(int *puntero);
instancia_t *simularDistribucion();

int enviarEncabezado(socket_t conexion, uint8_t valor);
//Devuelve 0 si envio bien, diferente en caso contrario
int enviarResultado(socket_t conexion, enum resultadoEjecucion result);
void enviarInfoEntradas(socket_t instancia);
char* instrToArray(enum instruccion instr);
void removerClave(instancia_t *instancia, char *clave);
void limpiarOperacion();
void salirConError(char*);
#endif /* SRC_COORDINADOR_H_ */
