#ifndef READY_H_
#define READY_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include "adminESI.h"
#include "configuracion.h" //Buscar solución para no importar la configuración acá

ESI* ESIPrioratarioPorAlgoritmo(enum t_algoritmo);
void listarParaEjecucion(ESI*);
void crearListaReady();
void cerrarListaReady();

#endif /* READY_H_ */
