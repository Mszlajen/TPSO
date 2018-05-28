#ifndef COLAS_EJECUTANDO_H_
#define COLAS_EJECUTANDO_H_

#include <stdio.h>
#include <biblio.h>
#include "../adminESI.h"

void ponerESIAEjecutar(ESI*);
void quitarESIEjecutando();
int esESIEnEjecucion(ESI_id);
ESI* ESIEjecutando();
void cerrarEjecutando();

#endif /* COLAS_EJECUTANDO_H_ */
