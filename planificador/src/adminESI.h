#ifndef ADMINESI_H_
#define ADMINESI_H_

#include <commons/collections/list.h>
#include <pthread.h>
#include <commons/string.h>
#include <biblio.h>

typedef struct {
	ESI_id id;
	socket_t socket;
	double estimacion;
	double ultimaEstimacion;
	unsigned long int arriboListos;
	pthread_t hiloDeFin;
	void* bufferAux;
	t_list* recursos;
}ESI;

ESI* crearESI (socket_t, int);

void destruirESI(ESI*);
void borrarRecursosESI(ESI*);

void actualizarEstimacion(ESI*);
void recalcularEstimacion(ESI*, double);
void ejecutarInstruccion(ESI*);
int calcularVejez(ESI*);
void agregarRecurso(ESI*, char*);
void quitarRecurso(ESI*, char*);

booleano esiEnLista(t_list*, ESI*);
#endif /* ADMINESI_H_ */
