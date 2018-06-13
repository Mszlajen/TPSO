#ifndef COLAS_EJECUTANDO_H_
#define COLAS_EJECUTANDO_H_

#include <stdio.h>
#include <biblio.h>
#include "../adminESI.h"

typedef struct {
	booleano hayQueBloquear;
	char* clave;
} avisoBloqueo_t;

void ponerESIAEjecutar(ESI*);
void quitarESIEjecutando();
int esESIEnEjecucion(ESI_id);
ESI* ESIEjecutando();
void settearAvisoBloqueo(char*);
booleano hayAvisoBloqueo();
char* getClaveAvisoBloqueo();
void cerrarEjecutando();

#endif /* COLAS_EJECUTANDO_H_ */
