#ifndef ADMINESI_H_
#define ADMINESI_H_

#include <commons/collections/list.h>
#include <biblio.h>

typedef struct {
	ESI_id id;
	socket_t socket;
	int estimacion;
	int estimacionAnterior;
	unsigned long int arriboListos;
	t_list *recursos;
}ESI;

ESI* crearESI (socket_t, int, unsigned long int);
void destruirESI(ESI*);

#endif /* ADMINESI_H_ */
