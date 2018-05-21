#include "planificador.h"
#include "configuracion.h"
#include "ready.h"
#include "adminESI.h"

socket_t socketCoord = ERROR, socketServerESI = ERROR;

int terminarEjecucion = 0;
/*
 * terminar ejecucion es una variable de Debuggeo
 * que se usa para terminar la ejecucion del planificador
 * dandole valor 1, sin embargo, actualmente es posible
 * que se necesite conectar un ESI antes de que finalice
 * realmente la ejecucion.
 */


int main(void) {
	inicializacion();

	conectarConCoordinador();

	enviarHandshake(socketCoord);

	crearServerESI();

	pthread_t hiloTerminal = crearHiloTerminal();

	pthread_t hiloESI = crearHiloNuevasESI();

	pthread_join(hiloTerminal, NULL);
	pthread_join(hiloESI, NULL);
	liberarRecursos();
	exit(0);
}

void inicializacion ()
{
	if(crearConfiguracion())
		salirConError("Fallo al leer el archivo de configuracion del planificador\n");
	crearListaReady();

}

void conectarConCoordinador()
{
	char * IP = obtenerDireccionCoordinador();
	char * puerto = obtenerPuertoCoordinador();
	socketCoord = crearSocketCliente(IP, puerto);
	if(socketCoord == ERROR)
		salirConError("Fallo al conectar planificador con coordinador\n");
}

void enviarHandshake(socket_t sock)
{
	header handshake;
	handshake.protocolo = 1;
	if(enviarHeader(sock, handshake) == ERROR)
		salirConError("Fallo al enviar el handshake planificador->Coordinador\n");
}

void crearServerESI ()
{
	char * puerto = obtenerPuerto();
	socketServerESI = crearSocketServer(IPConexion, puerto);
	if(socketServerESI == ERROR)
		salirConError("Planificador no pudo crear el socket para conectarse con ESI\n");
}

void terminal()
{
	while(!terminarEjecucion)
	{
		char * linea = readline("Comando:");
		//Aca va a ir el procesamiento para ver si es una instrucción
		//y su procesamiento correspondiente
		switch(convertirComando(linea))
		{
		case salir:
			terminarEjecucion = 1;
			break ;
		default:
			system(linea);
		}
		free(linea);
	}
	pthread_exit(NULL);
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
	while(!terminarEjecucion)
	{
		socketNuevaESI = ERROR;
		listen(socketServerESI, 5);
		socketNuevaESI = accept(socketServerESI, (struct sockaddr *) &infoDirr, &size_infoDirr);
		if(socketNuevaESI == ERROR)
		{
			error_show("Fallo en la conexion de ESI\n");
			continue;
		}
		nuevaESI = crearESI(socketNuevaESI);
		listarParaEjecucion(nuevaESI);
	}
	pthread_exit(NULL);
}

enum comandos convertirComando(char* linea)
{
	/*
	 * Para hacer despues.
	 * (Basicamente una secuencia de if)
	 */
	if(string_equals_ignore_case(linea, "salir"))
		return salir;
	return -1;
}

void liberarRecursos()
{
	eliminarConfiguracion();

	cerrarListaReady();

	cerrarSocket(socketCoord);
	cerrarSocket(socketServerESI);
}

void salirConError(char * error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}
