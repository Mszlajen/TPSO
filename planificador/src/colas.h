#ifndef COLAS_H_
#define COLAS_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include "adminESI.h"
#include "configuracion.h" //Buscar solución para no importar la configuración acá

ESI* seleccionarESIPorAlgoritmo(enum t_algoritmo);
void listarParaEjecucion(ESI*);
void finalizarESI(ESI*);
void crearListaReady();
void cerrarListaReady();

#endif /* COLAS_H_ */
