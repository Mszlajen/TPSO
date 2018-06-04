#include "esi.h"

t_config * configuracion = NULL;
FILE * programa = NULL;
socket_t socketCoord = ERROR, socketPlan = ERROR;

int main(int argc, char **argv) {
	inicializacion(argc, argv);

	conectarConCoordinador();

	conectarConPlanificador();

	esperarAvisoPlanificador(argv);

	puts("Salio todo bien\n");
	liberarRecursos();
	exit(0);
}

void inicializacion(int argc, char** argv)
{
	if(argc != 2)
		salirConError("Cantidad de parametros invalida\n");
	programa = abrirArchivoLectura(argv[1]);

	configuracion = config_create(archivoConfig);
	if(configuracion == NULL)
		salirConError("Fallo al abrir el archivo de lectura\n");
}

void conectarConCoordinador()
{
	char * IP = config_get_string_value(configuracion, IPCoord);
	char * puerto = config_get_string_value(configuracion, PuertoCoord);
	socketCoord = crearSocketCliente(IP, puerto);
	if(socketCoord == ERROR)
		salirConError("No se pudo conectar al coordinador\n");
	enviarHandshake(socketCoord);
}

void conectarConPlanificador()
{
	char * IP = config_get_string_value(configuracion, IPPlan);
	char * puerto = config_get_string_value(configuracion, PuertoPlan);
	socketPlan = crearSocketCliente(IP, puerto);
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


void esperarAvisoPlanificador(char** argv)
{
	int cantPeticiones = listen(socketPlan, 5);

	while(cantPeticiones){
		for(cantPeticiones;cantPeticiones>1;cantPeticiones--){

		programa = abrirArchivoLectura(argv[1]);

		//pasar Al Parser = fread(argv, sizeof(cada linea),1,programa);

		}
	}
}


void enviarInstruccionCoord()
{
	enviarProtocolo(socketCoord, 8);
}

int enviarProtocolo(socket_t sock, int protocolo)
{
	header handshake;
	handshake.protocolo = protocolo;
	return enviarHeader(sock, handshake);
}
