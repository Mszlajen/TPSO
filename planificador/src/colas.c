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
	agregarAFinalizadosESI(esi);
	if(esESIEnEjecucion(esi->id))
		quitarESIEjecutando(esi);
	else if(ESIEnReady(esi -> id))
		quitarESIDeReady(esi -> id);
	else
		quitarESIDeBloqueados(esi -> id);
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
	switch(algoritmo)
	{
	case sjf:
		if(ESIEjecutando())
			return ESIEjecutando();
		else
		{
			ponerESIAEjecutar(encontrarPorSJF());
			return ESIEjecutando();
		}
	case srt:
		//Está hecho fideo, arreglar.
		if(ESIEjecutando())
		{
			if(!getFlagNuevos())
				return ESIEjecutando();
			else
			{
				listarParaEjecucion(ESIEjecutando());
				quitarESIEjecutando();
				ponerESIAEjecutar(encontrarPorSJF());
				return ESIEjecutando();
			}
		}
		else
		{
			ponerESIAEjecutar(encontrarPorSJF());
			return ESIEjecutando();
		}
	case hrrn:
		if(ESIEjecutando())
			return ESIEjecutando();
		else
		{
			ponerESIAEjecutar(encontrarPorHRRN());
			return ESIEjecutando();

		}
	default: //Devuelve FCFS
		if(ESIEjecutando())
			return ESIEjecutando();
		else
		{
			ponerESIAEjecutar(encontrarPorFCFS());
			return ESIEjecutando();
		}
	}
}
