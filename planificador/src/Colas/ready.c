#include "ready.h"

t_list * listos = NULL;

void listarParaEjecucion (ESI* nuevaESI)
{
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

ESI* encontrarPorFCFS()
{
	ESI* res = list_get(listos, 0);
	list_remove(listos, 0);
	return res;
}

ESI* encontrarPorSJF()
{
	posicionESI res;
	res.esi = list_get(listos, 0);
	res.indice = 0;
	void compararESI(void* esi)
	{
		if(((ESI*) esi) -> estimacion < res.esi -> estimacion)
		{
			res.esi = (ESI*) esi;
			res.indice++;
		}
	}
	list_iterate(listos, compararESI);
	list_remove(listos, res.indice);
	return res.esi;
}

void crearListaReady()
{
	if(!listos)
		listos = list_create();

}

void cerrarListaReady()
{
	void removedorESIListos(void* esi)
	{
		removedorESI((ESI*) esi, listos);
	}

	if(listos)
	{
		list_destroy_and_destroy_elements(listos, removedorESIListos);
	}
}
