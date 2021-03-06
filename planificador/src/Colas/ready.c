#include "ready.h"

t_list * listos = NULL;
booleano flagNuevos = 0;

void listarParaEjecucion (ESI* nuevaESI)
{
	flagNuevos = 1;
	list_add(listos, nuevaESI);
}

ESI* ESIEnReady(ESI_id idESI)
{
	bool esESIporId(void* esi)
	{
		return ((ESI*) esi) -> id == idESI;
	}
	return list_find(listos, esESIporId);
}

void quitarESIDeReady(ESI_id idAQuitar)
{
	int indiceActual = 0, indice = -1;
	void encontrarIndice(void * esi)
	{
		if(idAQuitar != ((ESI*) esi) ->id)
			indiceActual++;
		else
			indice = indiceActual;

	}
	list_iterate(listos, encontrarIndice);
	if(indice != -1)
		list_remove(listos, indice);
}

ESI* encontrarPorFCFS()
{
	ESI* res = list_get(listos, 0);
	list_remove(listos, 0);
	return res;
}

ESI* encontrarPorSJF()
{
	int indice = 0;
	posicionESI res;
	res.esi = list_get(listos, 0);
	res.indice = indice;
	void compararESI(void* esi)
	{
		if(((ESI*) esi) -> estimacion < res.esi -> estimacion)
		{
			res.esi = (ESI*) esi;
			res.indice = indice;
		}
		indice++;
	}
	list_iterate(listos, compararESI);
	list_remove(listos, res.indice);
	return res.esi;
}

ESI* encontrarPorHRRN()
{
	int indice = 0;
	posicionESI res;
	res.esi = list_get(listos, 0);
	res.indice = indice;
	res.responseRatio = RR(res.esi, calcularVejez(res.esi));
	void compararESI(void* esi)
	{
		int responseRatioESI = RR(((ESI*) esi), calcularVejez((ESI*) esi));
		if(res.responseRatio < responseRatioESI)
		{
			res.esi = (ESI*) esi;
			res.indice = indice;
		}
		indice++;
	}
	list_iterate(listos, compararESI);
	list_remove(listos, res.indice);
	return res.esi;
}
void setFlagNuevos(uint8_t valor)
{
	flagNuevos = valor;
}

uint8_t getFlagNuevos()
{
	return flagNuevos;
}

booleano readyVacio()
{
	return list_is_empty(listos);
}

void crearListaReady()
{
	if(!listos)
		listos = list_create();

}

void cerrarListaReady()
{
	if(listos)
	{
		list_destroy_and_destroy_elements(listos, destruirESI);
	}
}
