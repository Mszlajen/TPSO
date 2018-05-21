#include "adminESI.h"
int ESItotales = 0;
long int instruccionesEjecutadas = 0;

ESI* crearESI (socket_t sock)
{
	ESI* unaESI = malloc (sizeof(ESI));
	unaESI -> id = ESItotales;
	ESItotales++;
	unaESI -> estimacion = obtenerEstimacionInicial();
	unaESI -> estimacionAnterior = 0;
	unaESI -> arriboListos = instruccionesEjecutadas;
	//unaESI -> recursos = list_create();
	unaESI -> socket = sock;
	return unaESI;
}



void registrarEjecucion()
{
	instruccionesEjecutadas++;
}

unsigned long int valorInstEjecutadas()
{
	return instruccionesEjecutadas;
}
