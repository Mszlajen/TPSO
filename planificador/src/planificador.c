#include "planificador.h"

t_config * configuracion;

t_list * listos;

socket_t socketCoord, socketServerESI;

int ESItotales;


int main(void) {
	inicializacion();

	socketCoord = conectarConCoordinador();

	enviarHandshake(socketCoord);

	socketServerESI = crearSocketServer("127.0.0.2", config_get_string_value(configuracion, "Puerto"));

	pthread_t hiloTerminal = crearHiloTerminal();

	pthread_t hiloESI = crearHiloNuevasESI();
	pthread_join(hiloTerminal, NULL);
	pthread_join(hiloESI, NULL);
}

void inicializacion ()
{
	configuracion = config_create("planificador.config");
	if(configuracion == NULL)
	{
		error_show("Fallo al leer el archivo de configuracion del planificador\n");
		salir_agraciadamente(1);
	}
	listos = list_create();

}

socket_t conectarConCoordinador()
{
	char * IP = config_get_string_value(configuracion, "IPCoord");
	char * puerto = config_get_string_value(configuracion, "PuertoCoord");
	return crearSocketCliente(IP, puerto);
}

void enviarHandshake(socket_t sock)
{
	header handshake;
	handshake.protocolo = 1;
	enviarHeader(sock, handshake);
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
