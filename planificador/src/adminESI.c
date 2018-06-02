#include "adminESI.h"

unsigned long int instruccionesEjecutadas = 0;

id_t ESItotales = 0;

ESI* crearESI (socket_t sock, int estimacionInicial)
{
	ESI* unaESI = malloc (sizeof(ESI));
	unaESI -> id = ESItotales + 1;
	ESItotales++;
	unaESI -> estimacion = estimacionInicial;
	unaESI -> ultimaEstimacion = estimacionInicial;
	unaESI -> arriboListos = instruccionesEjecutadas;
	unaESI -> recursos = list_create();
	unaESI -> socket = sock;
	return unaESI;
}

void actualizarEstimacion(ESI* esi)
{
	esi -> estimacion -= 1;
}

void recalcularEstimacion(ESI* esi, int alfa)
{
	int ultimaEjecucion = esi-> ultimaEstimacion - esi -> estimacion;
	esi -> estimacion = esi -> ultimaEstimacion * (1 - alfa) + ultimaEjecucion * alfa;
	esi -> ultimaEstimacion = esi -> estimacion;
}

void ejecutarInstruccion(ESI* esi)
{
	actualizarEstimacion(esi);
	instruccionesEjecutadas += 1;
}

int calcularVejez(ESI* esi)
{
	return (int) instruccionesEjecutadas - esi -> arriboListos;
}

void actualizarArribo(ESI* esi)
{
	esi -> arriboListos = instruccionesEjecutadas;
}

void agregarRecurso(ESI* esi, char* clave)
{
	list_add(esi -> recursos, clave);
}

void borrarRecursosESI(ESI* esi)
{
	list_iterate(esi -> recursos, free);
}

void destruirESI(ESI* esi)
{
	close(esi -> socket);
	list_destroy(esi -> recursos);
	free(esi);
}

void removedorESI(ESI* esi, t_list* lista)
{
	list_iterate(lista, borrarRecursosESI);
	destruirESI(esi);
}