#include "ready.h"

t_list * listos = NULL;
ESI* enEjecucion = NULL;

void listarParaEjecucion (ESI* nuevaESI)
{
	list_add(listos, nuevaESI);
}

ESI* seleccionarESIPorAlgoritmo(enum t_algoritmo algoritmo)
{
	resultadoAlgoritmo res;
	switch(algoritmo)
	{
	case sjf:
		if(enEjecucion)
			return enEjecucion;
		else
		{
			res = encontrarPorSJF();
			list_remove(listos, res.indice);
			enEjecucion = res.paraEjecutar;
			return res.paraEjecutar;
		}
	case srt:
	case hrrn:
	default: //Devuelve siempre FCFS
		enEjecucion = list_get(listos, 0);
		list_remove(listos, 0);
		return enEjecucion;
	}
}

int ESIEnReady(ESI_id idESI)
{
	int esESIporId(void* esi)
		{
			return ((ESI*) esi) -> id == idESI;
		}
	int resultado = 0;

	if(enEjecucion)
		resultado = esESIporId(enEjecucion);
	if(!resultado)
		resultado = list_any_satisfy(listos, esESIporId);
	return resultado;
}

resultadoAlgoritmo encontrarPorSJF()
{
	resultadoAlgoritmo res;
	res.paraEjecutar = list_get(listos, 0);
	res.indice = 0;
	void compararESI(void* esi)
	{
		if(((ESI*) esi) -> estimacion < res.paraEjecutar -> estimacion)
		{
			res.paraEjecutar = (ESI*) esi;
			res.indice++;
		}
	}
	list_iterate(listos, compararESI);
	return res;
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
