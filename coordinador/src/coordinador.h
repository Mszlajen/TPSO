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

typedef struct {
	socket_t socket;
	int idinstancia;
	/*El nombre sigue definido en diez caracteres
	 * maximo cuando no tiene limite limite definido
	 * [MATI]
	 */
	char* nombre;
} Instancia;

typedef struct {
	socket_t socket;
	enum instruccion instr;
	/*
	 * La clave y el valor son cadenas de caracteres,
	 * usar void* te lo va a complicar a la larga.
	 * [MATI]
	 */
	char* clave;
	char* valor;
	// como estan definidos los resultados de ejecucion?
	/*
	 * No lo definimos, lo pueden proponer ustedes y lo
	 * anotan en el archivo de protocolo.
	 * [MATI]
	 */
} Esi;

#define IPEscucha "127.0.0.2"
#define archivoConfig "coordinador.config"
#define Puerto "Puerto"

void hiloDeInstancia();
void esESIoInstancia (socket_t socketAceptado,struct sockaddr_in dir);
int esperarYaceptar(socket_t socketCoordinador,struct sockaddr_in* dir);
int validarPlanificador (socket_t socket);
void liberarRecursos();
void salirConError(char * error);
void inicializacion();
void atenderEsi(socket_t socket);
void registrarEsi(socket_t socket);
void registrarInstancia(socket_t socket);
void escucharPorAcciones ();
void setearReadfdsInstancia (Instancia instancia);
void setearReadfdsEsi (Esi esi);
void escucharReadfdsInstancia (Instancia  instancia);
void escucharReadfdsEsi (Esi esi);
void recibirConexiones();







#endif /* SRC_COORDINADOR_H_ */
