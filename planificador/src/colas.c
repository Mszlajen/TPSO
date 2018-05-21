#include "colas.h"

t_list * listos = NULL;
ESI* enEjecucion;

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
		return list_get(listos, 0);
	}
}

void crearListaReady()
{
	if(listos == NULL)
		listos = list_create();
}
void cerrarListaReady()
{
	if(listos != NULL)
		list_destroy(listos);
}
