#include "ejecutando.h"

ESI* ejecutando = NULL;

void ponerESIAEjecutar(ESI* esi)
{
	ejecutando = esi;
}

void quitarESIEjecutando()
{
	ejecutando = NULL;
}

int esESIEnEjecucion(ESI_id idESI)
{
	return ejecutando -> id == idESI;
}

ESI* ESIEjecutando ()
{
	return ejecutando;
}

void cerrarEjecutando()
{
	if(ejecutando)
		free(ejecutando);
}
