#include "ready.h"

t_list * listos = NULL;
ESI* enEjecucion = NULL;

void listarParaEjecucion (ESI* nuevaESI)
{
	list_add(listos, nuevaESI);
}

ESI* seleccionarESIPorAlgoritmo(enum t_algoritmo algoritmo)
{
	switch(algoritmo)
	{
	case sjf:
	case srt:
	case hrrn:
	default: //Devuelve siempre FCFS
		enEjecucion = list_get(listos, 0);
		list_remove(listos, 0);
		return enEjecucion;
	}
}

void crearListaReady()
{
	if(!listos)
		listos = list_create();
}

void cerrarListaReady()
{
	if(listos)
		list_destroy(listos);
}


