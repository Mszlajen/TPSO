#ifndef COLAS_BLOQUEADOS_H_
#define COLAS_BLOQUEADOS_H_

#include <stdio.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "../adminESI.h"

int ESIEstaBloqueadoPorClave(ESI*, char*);
//Devuelve true si la tiene
int ESITieneClave(ESI*, char*);
//Devuelve true si esta tomada
int claveTomada (char*);
//Devuelve true si esta tomada
int claveTomadaPorESI (char*, ESI*);
void reservarClave(ESI*, char*);
void reservarClaveSinESI(char*);
ESI* liberarClave(char*);
ESI* desbloquearESIDeClave(char*);
void crearColasBloqueados();
void cerrarColasBloqueados();
int ESIEstaEnLista(ESI*, t_list*);
void colocarEnColaESI(ESI*, char*);
t_list* listarID(char*);
#endif /* COLAS_BLOQUEADOS_H_ */
