#include "planificador.h"

t_config * configuracion;

t_list * listos;

int socketCoord, socketServerESI, ESItotales;


int main(void) {
	inicializacion("planificador.config");

	socketCoord = conectarConCoordinador();

	enviarHandshake(socketCoord);

	socketServerESI = crearSocketServer("127.0.0.2", config_get_string_value(configuracion, "Puerto"));

	crearHiloTerminal();

	crearHiloNuevasESI();

}

void inicializacion (char * archivoConf)
{
	configuracion = config_create(archivoConf);
	listos = list_create();

}

int conectarConCoordinador()
{
	char * IP = config_get_string_value(configuracion, "IPCoord");
	char * puerto = config_get_string_value(configuracion, "PuertoCoord");
	return crearSocketCliente(IP, puerto);
}

void enviarHandshake(int sock)
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
		system(linea);
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
	int socketNuevaESI, size_infoDirr = sizeof(struct sockaddr_storage);
	struct sockaddr_storage infoDirr;
	ESI* nuevaESI;
	while(1)
	{
		socketNuevaESI = 0;
		listen(socketServerESI, 5);
		socketNuevaESI = accept(socketServerESI, (struct sockaddr *) &infoDirr, &size_infoDirr);
		if(socketNuevaESI == ERROR)
		{
			error_show("Fallo en la conexion de ESI");
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
