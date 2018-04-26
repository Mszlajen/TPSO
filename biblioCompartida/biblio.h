

#ifndef BIBLIO_H_
#define BIBLIO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#define ERROR -1

typedef struct {
	short int protocolo;
} header;

void salir_agraciadamente(int);
int crearSocketCliente(char *, char *);
FILE* abrirArchivoLectura(char *);
void enviarAvisoSimple (int, short int);

#endif /* BIBLIO_H_ */
