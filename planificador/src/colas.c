#include "colas.h"

t_list * listos = NULL;
t_list * finalizados = NULL;
t_dictionary * colasBloqueados = NULL; //(Clave, lista de ESI)
t_dictionary * tablaBloqueos = NULL; //(Clave. ESI)
ESI* enEjecucion = NULL;

void listarParaEjecucion (ESI* nuevaESI)
{
	list_add(listos, nuevaESI);
}

int ESIEstaBloqueadoPorClave(ESI* esi, char* clave)
{
	if(dictionary_has_key(colasBloqueados, clave))
		return ESIEstaEnLista(esi, dictionary_get(colasBloqueados, clave));
	else
		return 0;
}

//Algo fideo, revisar luego;
int ESITieneClave(ESI* esi, char* clave)
{
	//Comprueba que alguien tenga es clave tomada
	if(dictionary_has_key(tablaBloqueos, clave))
	{
		ESI* poseedor = dictionary_get(tablaBloqueos, clave);
		//Comprueba que el que la tiene tomada es el que estoy preguntando
		if(esi -> id == poseedor -> id)
			return 0;
		else
		{
			bloquearESI(esi, clave);
			return 1;
		}
	}
	else
	{
		//Le da la clave al ESI
		dictionary_put(tablaBloqueos, clave, esi);
		return 0;
	}
}

void bloquearESI(ESI* esi, char* clave)
{
	if(dictionary_has_key(colasBloqueados, clave))
		list_add((t_list*) dictionary_get(colasBloqueados, clave), esi);
	else
	{
		t_list * listaDeClave = list_create();
		list_add(listaDeClave, esi);
		dictionary_put(colasBloqueados, clave, listaDeClave);
	}
}

/*
 * La función tiene el riesgo que si se finaliza el ESI en ejecución
 * el puntero enEjecucion queda con un valor de memoria sin permisos.
 */
void finalizarESI(ESI* esi)
{
	ESI_id * id = malloc(sizeof(ESI_id));
	*id = esi -> id;
	list_add(finalizados, id);
	liberarRecursosDeESI(esi);
	destruirESI(esi);
}

void liberarRecursosDeESI(ESI* esi)
{
	list_iterate(esi -> recursos, liberarRecurso);
}

void liberarRecurso(void* clave)
{
	dictionary_remove(tablaBloqueos, (char*) clave);
	if(dictionary_has_key(colasBloqueados, clave))
	{
		t_list* colaDeClave = dictionary_get(colasBloqueados, clave);
		ESI* desbloqueado = list_get(colaDeClave, 0);
		list_remove(colaDeClave, 0);
		listarParaEjecucion(desbloqueado);
	}
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

void crearColasBloqueados()
{
	if(!colasBloqueados)
		colasBloqueados = dictionary_create();
}

void cerrarColasBloqueados()
{
	if(colasBloqueados)
		dictionary_destroy(colasBloqueados);
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

int ESIEstaEnLista(ESI* esi, t_list * lista)
{
	bool esEsteESI(void* data)
	{
		return ((ESI*) data) -> id == esi -> id;
	}
	return list_any_satisfy(lista, &esEsteESI);
}
