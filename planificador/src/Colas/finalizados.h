#ifndef COLAS_FINALIZADOS_H_
#define COLAS_FINALIZADOS_H_

#include <stdio.h>
#include <commons/collections/list.h>
#include "../adminESI.h"

void agregarAFinalizadosESI(ESI*);
booleano estaEnFinalizados(ESI_id);

void crearListaFinalizados();
void cerrarListaFinalizados();

#endif /* COLAS_FINALIZADOS_H_ */
