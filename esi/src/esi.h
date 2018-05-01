#ifndef ESI_H_
#define ESI_H_

#include <stdio.h>
#include <stdlib.h>
#include <parsi/parser.h>
#include <commons/config.h>
#include <commons/error.h>
#include <biblio.h>

void inicializacion(int, char**);
void conectarConCoordinador();
void conectarConPlanificador();

FILE* abrirArchivoLectura(char*);
void enviarHandshake (socket_t);
void hacerLiberaciones();
void salirConError(char*);

#endif /* ESI_H_ */
