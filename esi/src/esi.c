#include "esi.h"

t_config * configuracion = NULL;
FILE * programa = NULL;
socket_t socketCoord = ERROR, socketPlan = ERROR;

int main(int argc, char **argv) {
	inicializacion(argc, argv);

	conectarConCoordinador();

	conectarConPlanificador();

	puts("Salio todo bien\n");
	liberarRecursos();
	exit(0);
}

void inicializacion(int argc, char** argv)
{
	if(argc != 2)
		salirConError("Cantidad de parametros invalida\n");
	programa = abrirArchivoLectura(argv[1]);

	configuracion = config_create("ESI.config");
	if(configuracion == NULL)
		salirConError("Fallo al abrir el archivo de lectura\n");
}

void conectarConCoordinador()
{
	socketCoord = crearSocketCliente(config_get_string_value(configuracion, "IPCoord"), config_get_string_value(configuracion, "PuertoCoord"));
	if(socketCoord == ERROR)
		salirConError("No se pudo conectar al coordinador\n");
	enviarHandshake(socketCoord);
}

void conectarConPlanificador()
{
	socketPlan = crearSocketCliente(config_get_string_value(configuracion, "IPPlan"), config_get_string_value(configuracion, "PuertoPlan"));
	if(socketPlan == ERROR)
		salirConError("No se pudo conectar al coordinador\n");
	enviarHandshake(socketPlan);
}

FILE* abrirArchivoLectura(char* nombreArchivo)
{
	return fopen(nombreArchivo, "r");
}

void enviarHandshake(socket_t socket)
{
	header mensaje;
	mensaje.protocolo = 3;
	if(enviarHeader(socket, mensaje) == ERROR)
		salirConError("No se pudo enviar mensaje handshake\n");
}

void salirConError(char* error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}

void liberarRecursos()
{
	if(configuracion != NULL)
		config_destroy(configuracion);
	if(programa != NULL)
		fclose(programa);
	if(socketCoord != ERROR)
		cerrarSocket(socketCoord);
	if(socketPlan != ERROR)
		cerrarSocket(socketPlan);
}
