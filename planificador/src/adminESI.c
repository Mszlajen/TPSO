#include "adminESI.h"

unsigned long int instruccionesEjecutadas = 0;

ESI_id ESItotales = 0;

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
	list_add(esi -> recursos, string_duplicate(clave));
}

void quitarRecurso(ESI* esi, char* clave)
{
	int posicion = 0;
	bool esClave(void* recurso)
	{
		return string_equals_ignore_case((char*)recurso, clave);
	}
	list_remove_and_destroy_by_condition(esi -> recursos, esClave, free);
}

void borrarRecursosESI(ESI* esi)
{
	list_iterate(esi -> recursos, free);
}

void destruirESI(ESI* esi)
{
	close(esi -> socket);
	list_destroy_and_destroy_elements(esi -> recursos, free);
	free(esi);
}

booleano esiEnLista(t_list* lista, ESI* esi)
{
	bool esESI(void* elemento)
	{
		return esi -> id == ((ESI*) elemento) -> id;
	}
	return list_any_satisfy(lista, esESI);
}
