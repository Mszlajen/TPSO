#ifndef COLAS_H_
#define COLAS_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "adminESI.h"
#include "configuracion.h" //Buscar solución para no importar la configuración acá

ESI* seleccionarESIPorAlgoritmo(enum t_algoritmo);
void listarParaEjecucion(ESI*);
void finalizarESI(ESI*);

int ESIEstaBloqueadoPorClave(ESI*, char*);
int ESITieneClave(ESI*, char*);
void liberarRecursosDeESI(ESI*);

void crearListaReady();
void crearListaFinalizados();
void cerrarListaReady();
void cerrarListaFinalizados();
void crearColasBloqueados();
void cerrarColasBloqueados();

void crearLista(t_list**);
void cerrarLista(t_list**);

int ESIEstaEnLista(ESI*, t_list*);
void liberarRecurso(void*);
void bloquearESI(ESI*, char*);

#endif /* COLAS_H_ */
