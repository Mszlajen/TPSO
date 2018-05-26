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
//Devuelve true si la tiene
int ESITieneClave(ESI*, char*);
//Devuelve true si esta tomada
int claveTomada (char*);
//Devuelve true si esta tomada
int claveTomadaPorESI (char*, ESI*);
void reservarClave(ESI*, char*);
void liberarRecursosDeESI(ESI*);
void liberarClave(char*);

void crearListaReady();
void crearListaFinalizados();
void cerrarListaReady();
void cerrarListaFinalizados();
void crearColasBloqueados();
void cerrarColasBloqueados();

void crearLista(t_list**);
void cerrarLista(t_list**);

int ESIEstaEnLista(ESI*, t_list*);
void bloquearESI(ESI*, char*);

#endif /* COLAS_H_ */
