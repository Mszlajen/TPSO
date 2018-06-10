#include "bloqueados.h"

t_dictionary * colasBloqueados = NULL; //(Clave, lista de ESI)
t_dictionary * tablaBloqueos = NULL; //(Clave. ESI)

void reservarClave(ESI* esi, char* clave)
{
	dictionary_put(tablaBloqueos, clave, esi);
	agregarRecurso(esi, clave);
}
void reservarClaveSinESI(char *clave)
{
	dictionary_put(tablaBloqueos, clave, NULL);
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
	if(dictionary_has_key(tablaBloqueos, clave))
	{
		ESI* poseedor = dictionary_get(tablaBloqueos, clave);
		//Comprueba que el que la tiene tomada es el que estoy preguntando
		return esi -> id == poseedor -> id;
	}
	else return 1;
}

void colocarEnColaESI(ESI* esi, char* clave)
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

booleano claveTomada (char* clave)
{
	return dictionary_has_key(tablaBloqueos, clave);
}

booleano claveTomadaPorESI (char* clave, ESI_id ID)
{
	if(dictionary_has_key(tablaBloqueos, clave))
		return ((ESI*) dictionary_get(tablaBloqueos, clave) ) -> id == ID;
	else
		return 0;
}

ESI* liberarClave(char* clave)
{
	dictionary_remove(tablaBloqueos, clave);

	return desbloquearESIDeClave(clave);
}

ESI* desbloquearESIDeClave(char* clave)
{
	ESI* desbloqueado = NULL;
	if(dictionary_has_key(colasBloqueados, clave))
	{
		t_list* colaDeClave = dictionary_get(colasBloqueados, clave);
		if(!list_is_empty(colaDeClave))
		{
			desbloqueado = list_get(colaDeClave, 0);
			list_remove(colaDeClave, 0);
		}
	}
	return desbloqueado;
}

void quitarESIDeBloqueados(ESI_id idAQuitar)
{
	t_list * colaConESI = NULL;
	int indiceActual = 0, indice = -1, indiceAQuitar;

	void encontrarIndice(void * esi)
	{
		if(idAQuitar != ((ESI*) esi) ->id)
			indiceActual++;
		else
			indice = indiceActual;
	}
	void encontrarClaveConESI(char* clave, void* colaClave)
	{
		list_iterate((t_list*) colaClave, encontrarIndice);
		if(indice != -1)
		{
			colaConESI = (t_list*) colaClave;
			indiceAQuitar = indice;
		}
		indiceActual = 0;
		indice = -1;
	}

	dictionary_iterator(colasBloqueados, encontrarClaveConESI);
	if(colaConESI)
		list_remove(colaConESI, indiceAQuitar);

}

t_list* listarID(char *clave)
{
	void* convertirESIaID(void* esi)
	{
		ESI_id *id = malloc(sizeof(ESI_id));
		*id = ((ESI*) esi) -> id;
		return (void*) id;
	}
	t_list* listaDeID = NULL;
	if(!dictionary_has_key(colasBloqueados, clave))
		return listaDeID;
	listaDeID = list_map(dictionary_get(colasBloqueados, clave), convertirESIaID);
	return listaDeID;

}

void crearColasBloqueados()
{
	if(!colasBloqueados)
		colasBloqueados = dictionary_create();
}

void cerrarColasBloqueados()
{
	void removerBloqueados(void* lista)
	{
		//list_iterate((t_list*) lista, borrarRecursosESI);
		list_destroy_and_destroy_elements((t_list*) lista, destruirESI);
	}

	if(colasBloqueados)
	{
		dictionary_destroy_and_destroy_elements(colasBloqueados, removerBloqueados);
		/*
		 * No hace falta destruir los datos en la tabla
		 * porque necesariamente tienen que estar en la
		 * cola de Ready o en una cola de Bloqueados y
		 * por lo tanto son borrados ahí.
		 */
		dictionary_destroy(tablaBloqueos);
	}
}

int ESIEstaEnLista(ESI* esi, t_list * lista)
{
	bool esEsteESI(void* data)
	{
		return ((ESI*) data) -> id == esi -> id;
	}
	return list_any_satisfy(lista, &esEsteESI);
}
