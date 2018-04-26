#include "planificador.h"

t_config * configuracion;

t_list * listos;


int main(void) {
	configuracion = config_create("planificador.config");
	listos = list_create();
	int socketCoord = conectarConCoordinador();

	enviarHandshake(socketCoord);

	int socketServerESI = crearSocketServer("127.0.0.2", config_get_string_value(configuracion, "Puerto"));

	crearHiloTerminal();



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
		system(linea);
		free(linea);
	}
}

pthread_t crearHiloTerminal()
{
	pthread_t hilo;
	pthread_create(&hilo, NULL, (void*) terminal, NULL);
	return hilo;
}
