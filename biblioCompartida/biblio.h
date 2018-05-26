#ifndef BIBLIO_H_
#define BIBLIO_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "commons/error.h"

#define ERROR -1
#define EXITO 0

typedef int socket_t;
typedef int ESI_id;
typedef int instancia_id;

typedef struct {
	uint8_t protocolo;
} header;

enum instruccion {get, set, store};

void salir_agraciadamente(int);
//-1 en error o fileDescriptor en exito
socket_t crearSocketCliente(char *, char *);
//-1 en error o fileDescriptor en exito
socket_t crearSocketServer(char *, char *);
//0 para falso o 1 para verdad
int seDesconectoSocket(socket_t);
void usarPuertoTapado (socket_t);

//-1 en error o 0 en exito
int enviarHeader (socket_t, header);
//-1 en error o 0 en exito
int enviarBuffer (socket_t, void*, int);
/*
 * Al tercer parametro de recibirMensaje
 * se le pasa un puntero por referencia
 * que se va a redireccionar a un buffer
 * en memoria creado por la funcion.
 *
 * -1 en error, 0 en exito o 1 si se cerro
 * el socket mientras recibia.
 */
int recibirMensaje (socket_t, int, void**);

/*
 * Si el socket no está iniciado (valor == ERROR)
 * lo cierra, no hace nada en caso contrario
 */
void cerrarSocket(socket_t);

#endif /* BIBLIO_H_ */
