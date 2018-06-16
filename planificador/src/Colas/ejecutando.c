#include "ejecutando.h"

ESI* ejecutando = NULL;
char* claveAvisoBloqueo;


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

void settearAvisoBloqueo(char* clave)
{
	claveAvisoBloqueo = clave;
}

booleano hayAvisoBloqueo()
{
	return claveAvisoBloqueo != NULL;
}

char* getClaveAvisoBloqueo()
{
	return claveAvisoBloqueo;
}

void cerrarEjecutando()
{
	if(ejecutando)
		free(ejecutando);
}
