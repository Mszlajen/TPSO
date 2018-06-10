#ifndef COLAS_BLOQUEADOS_H_
#define COLAS_BLOQUEADOS_H_

#include <stdio.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "../adminESI.h"

int ESIEstaBloqueadoPorClave(ESI*, char*);
int ESITieneClave(ESI*, char*);
int claveTomada (char*);
booleano claveTomadaPorESI (char*, ESI*);
void reservarClave(ESI*, char*);
void reservarClaveSinESI(char*);
ESI* liberarClave(char*);
ESI* desbloquearESIDeClave(char*);
void quitarESIDeBloqueados(ESI_id);
void crearColasBloqueados();
void cerrarColasBloqueados();
int ESIEstaEnLista(ESI*, t_list*);
void colocarEnColaESI(ESI*, char*);
t_list* listarID(char*);
#endif /* COLAS_BLOQUEADOS_H_ */
