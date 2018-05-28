#include "colas.h"

void inicializarColas()
{
	crearColasBloqueados();
	crearListaFinalizados();
	crearListaReady();
}

void cerrarColas()
{
	cerrarColasBloqueados();
	cerrarListaFinalizados();
	cerrarListaReady();
	cerrarEjecutando();
}

void finalizarESI(ESI* esi)
{
	liberarRecursosDeESI(esi);
	agregarAFinalizadosESI(esi);
	if(esESIEnEjecucion(esi->id))
		quitarESIEjecutando(esi);
	destruirESI(esi);
}

void liberarRecursosDeESI(ESI* esi)
{
	ESI* esiLiberado;
	while(list_is_empty(esi -> recursos))
	{
		 esiLiberado = liberarClave(list_get(esi -> recursos, 0));
		 if(esiLiberado)
			 listarParaEjecucion(esiLiberado);
		 list_remove(esi->recursos, 0);
	}
}

void bloquearESI(ESI* esi, char* clave)
{
	colocarEnColaESI(esi, clave);
	if(esESIEnEjecucion(esi->id))
		quitarESIEjecutando(esi);
}

ESI* seleccionarESIPorAlgoritmo(enum t_algoritmo algoritmo)
{
	ESI* paraEjecutar;
	switch(algoritmo)
	{
	case sjf:
		if((paraEjecutar = ESIEjecutando()))
			return paraEjecutar;
		else
		{
			ponerESIAEjecutar(encontrarPorSJF());
			return ESIEjecutando();
		}
	case srt:
	case hrrn:
	default: //Devuelve FCFS
		if((paraEjecutar = ESIEjecutando()))
			return paraEjecutar;
		else
		{
			ponerESIAEjecutar(encontrarPorFCFS());
			return ESIEjecutando();
		}
	}
}
