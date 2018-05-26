#ifndef READY_H_
#define READY_H_

#include <stdio.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "../adminESI.h"
#include "../configuracion.h"

ESI* seleccionarESIPorAlgoritmo(enum t_algoritmo);
void listarParaEjecucion(ESI*);

void crearListaReady();
void cerrarListaReady();
#endif /* READY_H_ */
