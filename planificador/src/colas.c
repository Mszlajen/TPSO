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
}

void finalizarESI(ESI* esi)
{
	liberarRecursosDeESI(esi);
	agregarAFinalizadosESI(esi);
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
	quitarESIEjecutando(esi);
}
