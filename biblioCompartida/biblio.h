

#ifndef BIBLIO_H_
#define BIBLIO_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <commons/error.h>

#define ERROR -1
#define EXITO 0

typedef int socket_t;

typedef struct {
	uint8_t protocolo;
} header;

void salir_agraciadamente(int);
socket_t crearSocketCliente(char *, char *);
socket_t crearSocketServer(char *, char *);
void usarPuertoTapado (socket_t);
/*
 * Todas las funciones de envio y
 * recepcion devuelven 0 para exito
 * y -1 para error.
 */
int enviarHeader (socket_t, header);
int enviarBuffer (socket_t, void*, int);
/*
 * Al tercer parametro de recibirMensaje
 * se le pasa un puntero donde se va a
 * redireccionar a un buffer en memoria
 * creado por la funcion.
 */
int recibirMensaje (socket_t, int, void**);

void cerrarSocket(socket_t);

#endif /* BIBLIO_H_ */
