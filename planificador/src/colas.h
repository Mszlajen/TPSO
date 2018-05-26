#ifndef COLAS_H_
#define COLAS_H_

#include <commons/collections/list.h>
#include "Colas/bloqueados.h"
#include "Colas/finalizados.h"
#include "Colas/ready.h"


void liberarRecursosDeESI(ESI*);
void finalizarESI(ESI*);


#endif /* COLAS_H_ */
