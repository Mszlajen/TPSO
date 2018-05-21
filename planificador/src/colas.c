#include "colas.h"

t_list * listos = NULL;
t_list * finalizados = NULL;
ESI* enEjecucion = NULL;

void listarParaEjecucion (ESI* nuevaESI)
{
	list_add(listos, nuevaESI);
}

void finalizarESI(ESI* esi)
{
	ESI_id * id = malloc(sizeof(ESI_id));
	*id = esi -> id;
	list_add(finalizados, id);
	destruirESI(esi);
	enEjecucion = NULL;
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
	crearLista(&listos);
}

void cerrarListaReady()
{
	cerrarLista(&listos);
}

void crearListaFinalizados()
{
	crearLista(&finalizados);
}

void cerrarListaFinalizados()
{
	cerrarLista(&finalizados);
}

void crearLista(t_list** lista)
{
	if(!lista)
		*lista = list_create();
}

void cerrarLista(t_list** lista)
{
	if(lista)
		list_destroy(*lista);
}
