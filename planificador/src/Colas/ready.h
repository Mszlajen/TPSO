#ifndef READY_H_
#define READY_H_

#include <stdio.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "../adminESI.h"
#include "../configuracion.h"

#define RR(pESI, vejez) (vejez / pESI -> estimacion)

typedef struct {
	ESI* esi;
	int indice;
	int responseRatio;
}posicionESI;

void listarParaEjecucion(ESI*);

/*Los algoritmos quitan al ESI de la cola*/
ESI* encontrarPorFCFS();
ESI* encontrarPorSJF();
ESI* encontrarPorHRRN();

booleano readyVacio();
ESI* ESIEnReady(ESI_id);

void quitarESIDeReady(ESI_id);

void setFlagNuevos(uint8_t);
uint8_t getFlagNuevos();

void crearListaReady();
void cerrarListaReady();
#endif /* READY_H_ */
