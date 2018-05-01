#include "planificador.h"

t_config * configuracion = NULL;

t_list * listos = NULL;

socket_t socketCoord = ERROR, socketServerESI = ERROR;

int ESItotales;


int main(void) {
	inicializacion();

	conectarConCoordinador();

	enviarHandshake(socketCoord);

	crearServerESI();

	pthread_t hiloTerminal = crearHiloTerminal();

	pthread_t hiloESI = crearHiloNuevasESI();
	pthread_join(hiloTerminal, NULL);
	pthread_join(hiloESI, NULL);
}

void inicializacion ()
{
	configuracion = config_create("planificador.config");
	if(configuracion == NULL)
		salirConError("Fallo al leer el archivo de configuracion del planificador\n");
	listos = list_create();

}

void conectarConCoordinador()
{
	char * IP = config_get_string_value(configuracion, "IPCoord");
	char * puerto = config_get_string_value(configuracion, "PuertoCoord");
	socketCoord = crearSocketCliente(IP, puerto);
	if(socketCoord == ERROR)
		salirConError("Fallo al conectar planificador con coordinador\n");
}

void enviarHandshake(socket_t sock)
{
	header handshake;
	handshake.protocolo = 1;
	enviarHeader(sock, handshake);
}

void crearServerESI ()
{
	char * puerto = config_get_string_value(configuracion, "Puerto");
	socketServerESI = crearSocketServer("127.0.0.2", puerto);
	if(socketServerESI == ERROR)
		salirConError("Planificador no pudo crear el socket para conectarse con ESI");
}

void terminal()
{
	while(1)
	{
		char * linea = readline("");
		//Aca va a ir el procesamiento para ver si es una instrucción
		//y su procesamiento correspondiente
		switch(convertirComando(linea))
		{
		default:
			system(linea);
		}
		free(linea);
	}
}

/*
 * Las siguientes funciones repiten codigo pero
 * no creo que sea buen momento para aprender
 * a crear funciones de orden superior en C
 */
pthread_t crearHiloTerminal()
{
	pthread_t hilo;
	pthread_create(&hilo, NULL, (void*) terminal, NULL);
	return hilo;
}

pthread_t crearHiloNuevasESI()
{
	pthread_t hilo;
	pthread_create(&hilo, NULL, (void*) escucharPorESI, NULL);
	return hilo;
}

void escucharPorESI ()
{
	int socketNuevaESI;
	struct sockaddr_storage infoDirr;
	socklen_t size_infoDirr = sizeof(struct sockaddr_storage);
	ESI* nuevaESI;
	while(1)
	{
		socketNuevaESI = 0;
		listen(socketServerESI, 5);
		socketNuevaESI = accept(socketServerESI, (struct sockaddr *) &infoDirr, &size_infoDirr);
		if(socketNuevaESI == ERROR)
		{
			error_show("Fallo en la conexion de ESI\n");
			continue;
		}
		nuevaESI = crearESI(socketNuevaESI);
		list_add(listos, nuevaESI);
	}
}

ESI* crearESI (int sock)
{
	ESI* unaESI = malloc (sizeof(ESI));
	unaESI -> id = ESItotales;
	ESItotales++;
	unaESI -> estimacion = config_get_int_value(configuracion, "Estimacion");
	unaESI -> recursos = list_create();
	unaESI -> socket = sock;
	return unaESI;
}

enum comandos convertirComando(char* linea)
{
	/*
	 * Para hacer despues.
	 * (Basicamente una secuencia de if)
	 */
	return -1;
}

void liberarRecursos()
{
	if(configuracion != NULL)
		config_destroy(configuracion);
	if(listos != NULL)
		list_destroy(listos);
	if(socketCoord != ERROR)
		cerrarSocket(socketCoord);
	if(socketServerESI != ERROR)
		cerrarSocket(socketServerESI);
}

void salirConError(char * error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}
