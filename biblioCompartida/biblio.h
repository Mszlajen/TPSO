

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

typedef struct {
	uint8_t protocolo;
} header;

void salir_agraciadamente(int);
socket_t crearSocketCliente(char *, char *); //-1 en error o fileDescriptor en exito
socket_t crearSocketServer(char *, char *); //-1 en error o fileDescriptor en exito
void usarPuertoTapado (socket_t);

int enviarHeader (socket_t, header); //-1 en error o 0 en exito
int enviarBuffer (socket_t, void*, int); //-1 en error o 0 en exito
/*
 * Al tercer parametro de recibirMensaje
 * se le pasa un puntero por referencia
 * que se va a redireccionar a un buffer
 * en memoria creado por la funcion.
 */
int recibirMensaje (socket_t, int, void**); //-1 en error o 0 en exito

void cerrarSocket(socket_t);

#endif /* BIBLIO_H_ */
