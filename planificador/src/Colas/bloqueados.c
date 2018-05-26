#include "bloqueados.h"

t_dictionary * colasBloqueados = NULL; //(Clave, lista de ESI)
t_dictionary * tablaBloqueos = NULL; //(Clave. ESI)

void reservarClave(ESI* esi, char* clave)
{
	dictionary_put(tablaBloqueos, clave, esi);
	agregarRecurso(esi, clave);
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
	//Comprueba si alguien tiene la clave tomada
	if(dictionary_has_key(tablaBloqueos, clave))
	{
		ESI* poseedor = dictionary_get(tablaBloqueos, clave);
		//Comprueba que el que la tiene tomada es el que estoy preguntando
		return esi -> id == poseedor -> id;
	}
	else return 1;
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

int claveTomada (char* clave)
{
	return dictionary_has_key(tablaBloqueos, clave);
}

int claveTomadaPorESI (char* clave, ESI* esi)
{
	if(dictionary_has_key(tablaBloqueos, clave))
		return ((ESI*) dictionary_get(tablaBloqueos, clave) ) -> id == esi -> id;
	else
		return 0;
}

ESI* liberarClave(char* clave)
{
	dictionary_remove(tablaBloqueos, clave);
	ESI* desbloqueado = NULL;
	if(dictionary_has_key(colasBloqueados, clave))
	{
		t_list* colaDeClave = dictionary_get(colasBloqueados, clave);
		desbloqueado = list_get(colaDeClave, 0);
		list_remove(colaDeClave, 0);
	}
	return desbloqueado;
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

int ESIEstaEnLista(ESI* esi, t_list * lista)
{
	bool esEsteESI(void* data)
	{
		return ((ESI*) data) -> id == esi -> id;
	}
	return list_any_satisfy(lista, &esEsteESI);
}
