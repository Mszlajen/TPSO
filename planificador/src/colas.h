#ifndef COLAS_H_
#define COLAS_H_

#include <commons/collections/list.h>
#include "Colas/bloqueados.h"
#include "Colas/finalizados.h"
#include "Colas/ready.h"

void inicializarColas();
void cerrarColas();
void liberarRecursosDeESI(ESI*);
void finalizarESI(ESI*);
void bloquearESI(ESI*, char*);


#endif /* COLAS_H_ */
