#ifndef ADMINESI_H_
#define ADMINESI_H_

#include <commons/collections/list.h>
#include <pthread.h>
#include <commons/string.h>
#include <biblio.h>

typedef struct {
	id_t id;
	socket_t socket;
	int estimacion;
	int ultimaEstimacion;
	unsigned long int arriboListos;
	pthread_t hiloDeFin;
	t_list* recursos;
}ESI;

ESI* crearESI (socket_t, int);

void destruirESI(ESI*);
void borrarRecursosESI(ESI*);

void actualizarEstimacion(ESI*);
void recalcularEstimacion(ESI*, int);
void ejecutarInstruccion(ESI*);
int calcularVejez(ESI*);
void agregarRecurso(ESI*, char*);
void quitarRecurso(ESI*, char*);

booleano esiEnLista(t_list*, ESI*);
#endif /* ADMINESI_H_ */
