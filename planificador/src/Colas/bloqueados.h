#ifndef COLAS_BLOQUEADOS_H_
#define COLAS_BLOQUEADOS_H_

#include <stdio.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "../adminESI.h"

int ESIEstaBloqueadoPorClave(ESI*, char*);
int ESITieneClave(ESI*, char*);
//Devuelve NULL en false y la referencia al ESI en true
ESI* ESIEstaBloqueado(ESI_id);
booleano claveTomada (char*);
booleano claveTomadaPorESI (char*, ESI_id);
//Si el retorno es verdadero, el segundo parametro tiene la referencia al ESI o NULL (para el sistema).
//Si el retorno es falso, la clave no está tomada por nadie.
booleano claveTomadaPor(char*, ESI**);
void reservarClave(ESI*, char*);
void reservarClaveSinESI(char*);
ESI* liberarClave(char*);
ESI* desbloquearESIDeClave(char*);
void quitarESIDeBloqueados(ESI_id);
void crearColasBloqueados();
void cerrarColasBloqueados();
booleano ESIEstaEnLista(ESI*, t_list*);
void colocarEnColaESI(ESI*, char*);
t_list* listarID(char*);
t_list* detectarDeadlock();
void entregarRecursosAlSistema(ESI*);
#endif /* COLAS_BLOQUEADOS_H_ */
