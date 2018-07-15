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

void entregarRecursosAlSistema(ESI* esi)
{
	void guardarClaveDeSistema(void* clave)
	{
		dictionary_remove(tablaBloqueos, (char*) clave);
		reservarClaveSinESI((char*) clave);
	}
	list_iterate(esi->recursos, guardarClaveDeSistema);
	list_iterate(esi->recursos, free);
	list_clean(esi->recursos);
}

int ESIEstaBloqueadoPorClave(ESI* esi, char* clave)
{
	if(dictionary_has_key(colasBloqueados, clave))
		return ESIEstaEnLista(esi, dictionary_get(colasBloqueados, clave));
	else
		return 0;
}

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

ESI* ESIEstaBloqueado(ESI_id esiId)
{
	/*
	 * La falta optimicidad (eso es una palabra?)
	 * porque necesita revisar todos los ESI
	 * bloqueados siempre.
	 */
	ESI* retorno = NULL;
	void comprobarESI(void* esi)
	{
		if(((ESI*) esi) -> id == esiId)
			retorno = esi;
	}
	void buscarESIenCola(void* cola)
	{
		list_iterate((t_list*) cola, comprobarESI);
	}
	dictionary_iterator(colasBloqueados, buscarESIenCola);
	return retorno;
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

booleano claveTomadaPor(char* clave, ESI** tomador)
{
	if(dictionary_has_key(tablaBloqueos, clave))
	{
		*tomador = dictionary_get(tablaBloqueos, clave);
		return 1;
	}
	else
		return 0;
}

ESI* liberarClave(char* clave)
{
	ESI* poseedorActual = dictionary_get(tablaBloqueos, clave);
	if(dictionary_has_key(tablaBloqueos, clave))
		dictionary_remove(tablaBloqueos, clave);
	if(poseedorActual)
		quitarRecurso(poseedorActual, clave);
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
void* convertirESIaID(void* esi)
{
	ESI_id *id = malloc(sizeof(ESI_id));
	*id = ((ESI*) esi) -> id;
	return (void*) id;
}

t_list* listarID(char *clave)
{
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
	if(!tablaBloqueos)
		tablaBloqueos = dictionary_create();
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

booleano ESIEstaEnLista(ESI* esi, t_list * lista)
{
	bool esEsteESI(void* data)
	{
		return ((ESI*) data) -> id == esi -> id;
	}
	return list_any_satisfy(lista, &esEsteESI);
}

booleano existeCandidatoBloqueadoPorClave(t_list* candidatos, t_list* claves)
{
	int i_cand, i_clav;
	for(i_cand = 0; i_cand < list_size(candidatos); i_cand++)
	{
		for(i_clav = 0; i_clav < list_size(claves); i_clav++)
		{
			if(ESIEstaBloqueadoPorClave(list_get(candidatos, i_cand), list_get(claves, i_clav)))
				return 1;
		}
	}
	return 0;
}

t_list* detectarDeadlock()
{
	//Creo la lista de candidatos y el retorno para el caso de no haber deadlock.
	t_list *candidatos = list_create(), *retorno = NULL;

	//Coloco en la lista a los candidatos filtrando las claves bloqueadas por el sistema.
	//Solo puede estar una vez cada ESI.
	void filtrarClavesBloqueadasPorSistema(char* clave, void *esi)
	{
		if(esi && !esiEnLista(candidatos, (ESI*) esi))
			list_add(candidatos, esi);
	}
	dictionary_iterator(tablaBloqueos, filtrarClavesBloqueadasPorSistema);

	//Elimino a los candidatos que no bloquean a otros ESI y repito hasta que no puedo eliminar más.
	int cantCandidatosAnt;
	int i;
	ESI* candidatoActual = NULL;
	do
	{
		cantCandidatosAnt = list_size(candidatos);
		//Empiezo por el final para que al borrar los candidatos que no fueron, no se corra el puntero indice(i)
		for(i = list_size(candidatos) - 1; i >= 0; i--)
		{
			candidatoActual = list_get(candidatos, i);
			if(!existeCandidatoBloqueadoPorClave(candidatos, candidatoActual -> recursos))
				list_remove(candidatos, i);
		}
	}while(list_size(candidatos) > cantCandidatosAnt);

	//Compruebo si quedan candidatos, es decir, compruebo si hay deadlock y creo la lista con ID.
	if(!list_is_empty(candidatos))
		retorno = list_map(candidatos, convertirESIaID);

	list_destroy(candidatos);
	return retorno;

}


