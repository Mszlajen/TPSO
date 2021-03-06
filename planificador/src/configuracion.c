#include "configuracion.h"

t_config * configuracion = NULL;

int crearConfiguracion(char* archivoConfig)
{
	if(!configuracion)
		configuracion = config_create(archivoConfig);
	return configuracion? 0 : 1;
}

void eliminarConfiguracion()
{
	if(configuracion)
		config_destroy(configuracion);
}

char* obtenerIP()
{
	return config_get_string_value(configuracion, Ip);
}

char* obtenerPuerto()
{
	return config_get_string_value(configuracion, Puerto);
}

char* obtenerDireccionCoordinador()
{
	return config_get_string_value(configuracion, IPCoord);
}

char* obtenerPuertoCoordinador()
{
	return config_get_string_value(configuracion, PuertoCoord);
}

double obtenerAlfa()
{
	return config_get_int_value(configuracion, Alfa) / 100.0;
}

double obtenerEstimacionInicial()
{
	return config_get_double_value(configuracion, Estimacion);
}

enum t_algoritmo obtenerAlgoritmo()
{
	enum t_algoritmo resultado;
	char* sAlgort = config_get_string_value(configuracion, Algoritmo);
	if(string_equals_ignore_case(sAlgort, "SJF-CD"))
		resultado = srt;
	else if(string_equals_ignore_case(sAlgort, "SJF-SD"))
		resultado = sjf;
	else if(string_equals_ignore_case(sAlgort, "HRRN"))
		resultado = hrrn;
	return resultado;
}

char* obtenerBloqueosConfiguracion()
{
	return config_get_string_value(configuracion, ClavesBloq);
}

