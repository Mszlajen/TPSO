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
		if(enEjecucion)
			return enEjecucion;
		else
			return encontrarPorSJF();
	case srt:
	case hrrn:
	default: //Devuelve siempre FCFS
		enEjecucion = list_get(listos, 0);
		list_remove(listos, 0);
		return enEjecucion;
	}
}

ESI* encontrarPorSJF()
{
	ESI* proximoAEjecutar = list_get(listos, 0);
	void compararESI(void* esi)
	{
		if(((ESI*) esi) -> estimacion < proximoAEjecutar -> estimacion)
			proximoAEjecutar = (ESI*) esi;
	}
	list_iterate(listos, compararESI);
	return proximoAEjecutar;
}

void quitarESIEjecutando(ESI* esi)
{
	if(enEjecucion == esi)
		enEjecucion = NULL;
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


