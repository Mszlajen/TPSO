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

void crearListaFinalizados()
{
	if(!finalizados)
		finalizados = list_create();
}

void cerrarListaFinalizados()
{
	if(finalizados)
		list_destroy(finalizados);
}
