#include "configuracion.h"

t_config* configuracion = NULL;

int crearConfiguracion(char* direccion)
{
	if(!configuracion)
		configuracion = config_create(direccion);
	return configuracion? 0 : 1;
}

void destruirConfiguracion()
{
	if(configuracion)
		config_destroy(configuracion);
}

char* obtenerNombreInstancia()
{
	return config_get_string_value(configuracion, nombre);
}

char* obtenerIPCordinador()
{
	return config_get_string_value(configuracion, IPCoord);
}

char* obtenerPuertoCoordinador()
{
	return config_get_string_value(configuracion, puertoCoord);
}

char* obtenerPuntoDeMontaje()
{
	return config_get_string_value(configuracion, puntoMontaje);
}

int obtenerIntervaloDeDump()
{
	return config_get_int_value(configuracion, tiempoDump);
}

enum algoritmoReemplazo obtenerAlgoritmoReemplazo()
{
	char* nombreAlgoritmo = config_get_string_value(configuracion, algoReempl);
	if(string_equals_ignore_case(nombreAlgoritmo, "BSU"))
		return BSU;
	else if(string_equals_ignore_case(nombreAlgoritmo, "LSU"))
		return LRU;
	else
		return Circular;
}