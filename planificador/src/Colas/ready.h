#ifndef READY_H_
#define READY_H_

#include <stdio.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "../adminESI.h"
#include "../configuracion.h"

typedef struct {
	ESI* esi;
	int indice;
}posicionESI;

void listarParaEjecucion(ESI*);

/*Los algoritmos quitan al ESI de la cola*/
ESI* encontrarPorFCFS();
ESI* encontrarPorSJF();

booleano readyVacio();
ESI* ESIEnReady(ESI_id);

void quitarESIDeReady(ESI_id);

void setFlagNuevos(uint8_t);
uint8_t getFlagNuevos();

void crearListaReady();
void cerrarListaReady();
#endif /* READY_H_ */
