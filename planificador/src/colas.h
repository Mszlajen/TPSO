#ifndef COLAS_H_
#define COLAS_H_

#include <commons/collections/list.h>
#include "Colas/bloqueados.h"
#include "Colas/finalizados.h"
#include "Colas/ready.h"
#include "Colas/ejecutando.h"

void inicializarColas();
void cerrarColas();
ESI* seleccionarESIPorAlgoritmo(enum t_algoritmo);
void liberarRecursosDeESI(ESI*);
//No libera los recursos del ESI
void finalizarESI(ESI*);
void bloquearESI(ESI*, char*);


#endif /* COLAS_H_ */
