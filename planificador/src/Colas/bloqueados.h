#ifndef COLAS_BLOQUEADOS_H_
#define COLAS_BLOQUEADOS_H_

#include <stdio.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "../adminESI.h"
#include "ready.h"

int ESIEstaBloqueadoPorClave(ESI*, char*);
//Devuelve true si la tiene
int ESITieneClave(ESI*, char*);
//Devuelve true si esta tomada
int claveTomada (char*);
//Devuelve true si esta tomada
int claveTomadaPorESI (char*, ESI*);
void reservarClave(ESI*, char*);
ESI* liberarClave(char*);
void crearColasBloqueados();
void cerrarColasBloqueados();
int ESIEstaEnLista(ESI*, t_list*);
void bloquearESI(ESI*, char*);
#endif /* COLAS_BLOQUEADOS_H_ */
