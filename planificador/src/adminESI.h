#ifndef ADMINESI_H_
#define ADMINESI_H_

#include <commons/collections/list.h>
#include <biblio.h>

typedef struct {
	id_t id;
	socket_t socket;
	int estimacion;
	int ultimaEstimacion;
	unsigned long int arriboListos;
	t_list *recursos;
}ESI;

ESI* crearESI (socket_t, int);
/*
 * Antes de destruir hay que liberar sus recursos.
 */
void destruirESI(ESI*);
void borrarRecursosESI(ESI*);
void removedorESI(ESI*, t_list* lista);

void actualizarEstimacion(ESI*);
void recalcularEstimacion(ESI*, int);
void ejecutarInstruccion(ESI*);
int calcularVejez(ESI*);
void agregarRecurso(ESI*, char*);

#endif /* ADMINESI_H_ */
