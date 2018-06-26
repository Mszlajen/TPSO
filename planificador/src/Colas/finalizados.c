#include "finalizados.h"

t_list * finalizados = NULL;

/*
 * La función tiene el riesgo que si se finaliza el ESI en ejecución
 * el puntero enEjecucion queda con un valor de memoria sin permisos.
 */
void agregarAFinalizadosESI(ESI* esi)
{
	ESI_id * id = malloc(sizeof(ESI_id));
	*id = esi -> id;
	list_add(finalizados, id);
}

booleano estaEnFinalizados(ESI_id id)
{
	bool esESI(void *data)
	{
		return *((ESI_id*)data) == id;
	}

	return list_any_satisfy(finalizados, esESI);
}

void crearListaFinalizados()
{
	if(!finalizados)
		finalizados = list_create();
}

void cerrarListaFinalizados()
{
	if(finalizados)
	{
		list_destroy_and_destroy_elements(finalizados, free);
	}
}
