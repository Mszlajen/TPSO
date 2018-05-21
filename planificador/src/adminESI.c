#include "adminESI.h"

ESI_id ESItotales = 0;

ESI* crearESI (socket_t sock, int estimacionInicial, unsigned long int arribo)
{
	ESI* unaESI = malloc (sizeof(ESI));
	unaESI -> id = ESItotales;
	ESItotales++;
	unaESI -> estimacion = estimacionInicial;
	unaESI -> estimacionAnterior = 0;
	unaESI -> arriboListos = arribo;
	//unaESI -> recursos = list_create();
	unaESI -> socket = sock;
	return unaESI;
}


void destruirESI(ESI* esi)
{
	close(esi -> socket);
	free(esi);
}
