#include "planificador.h"
#include "configuracion.h"
#include "colas.h"

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

	crearServerESI();

	pthread_t hiloTerminal = crearHiloTerminal();

	pthread_t hiloESI = crearHiloNuevasESI();

	pthread_t hiloEjecucion = crearHiloEjecucion();

	pthread_join(hiloTerminal, NULL);
	pthread_join(hiloEjecucion, NULL);
	pthread_join(hiloESI, NULL);
	liberarRecursos();
	exit(0);
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
}

void ejecucionDeESI()
{
	ESI* enEjecucion;
	consultaCoord * consulta = NULL;
	while(!terminarEjecucion)
	{
		do
		{
			enEjecucion = seleccionarESIPorAlgoritmo(obtenerAlgoritmo());
			if(seDesconectoSocket(enEjecucion -> socket))
				finalizarESI(enEjecucion);
			else
				break;
		}while(1); //Está un poco spaghetti, revisar luego.
		if(seDesconectoSocket(socketCoord))
		{
			//Avisar a los ESI que tienen que morirse.
			salirConError("Se cerro el coordinador.");
		}
		enviarEncabezado(enEjecucion -> socket, 7); //Enviar el aviso de ejecucion
		consulta = recibirConsultaCoord();


	}
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
		nuevaESI = crearESI(socketNuevaESI, obtenerEstimacionInicial());
		if(!enviarIdESI(socketNuevaESI, nuevaESI -> id))
			listarParaEjecucion(nuevaESI);
		else
		{
			destruirESI(nuevaESI);
			error_show("Fallo en la conexion de ESI\n");
		}
	}
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
	if(!enviarEncabezado(socketCoord, 1)) //Enviar el handshake
		salirConError("Fallo al enviar el handshake planificador -> Coordinador\n");
}

int enviarEncabezado(socket_t sock, int prot)
{
	header handshake;
	handshake.protocolo = prot;
	return enviarHeader(sock, handshake);
}

int enviarIdESI(socket_t sock, ESI_id id)
{
	header respuesta;
	respuesta.protocolo = 6;
	int tamEncabezado = sizeof(header), tamId = sizeof(id_t);
	void* buffer = malloc(tamEncabezado + tamId);
	memcpy(buffer, &respuesta, tamEncabezado);
	memcpy(buffer + tamEncabezado, &id, tamId);
	int resultado = enviarBuffer(sock, buffer, sizeof(buffer));
	free(buffer);
	return resultado;
}

int enviarRespuestaConsultaCoord(socket_t coord, int respuesta)
{
	header encabezado;
	encabezado.protocolo = 10;
	void* buffer = malloc(sizeof(header) + sizeof(respuesta));
	memcpy(buffer, &encabezado, sizeof(header));
	memcpy(buffer + sizeof(header), &respuesta, sizeof(respuesta));
	return enviarBuffer(coord, buffer, sizeof(buffer));
}

consultaCoord* recibirConsultaCoord()
{
	listen(socketCoord, 5);
	consultaCoord *consulta = malloc(sizeof(consultaCoord));

	header* encabezado;
	recibirMensaje(socketCoord, sizeof(header), (void**) &encabezado);
	if(encabezado->protocolo != 9)
		/*error*/;
	free(encabezado);

	enum instruccion *instr;
	recibirMensaje(socketCoord, sizeof(enum instruccion), (void**) &instr);
	consulta -> instr = *instr;
	free(instr);

	uint8_t *tamClave;
	recibirMensaje(socketCoord, sizeof(uint8_t), (void**) &tamClave);
	consulta -> tamClave = *tamClave;
	free(tamClave);

	recibirMensaje(socketCoord, consulta -> tamClave, (void**) &consulta -> clave);

	return consulta;
}

int procesarConsultaCoord(ESI* ejecutando, consultaCoord* consulta)
{
	switch(consulta -> instr)
	{
	case get:
		if(claveTomada(consulta -> clave))
		{
			bloquearESI(ejecutando, consulta -> clave);
			return 0;
		}
		else
		{
			reservarClave(ejecutando, consulta -> clave);
			return 1;
		}

	case set:
		return claveTomadaPorESI(consulta -> clave, ejecutando);

	case store:
		if(claveTomadaPorESI(consulta -> clave, ejecutando))
		{
			liberarClave(consulta -> clave);
			return 1;
		}
		else
			return 0;
	}
}
void crearServerESI ()
{
	char * puerto = obtenerPuerto();
	socketServerESI = crearSocketServer(IPConexion, puerto);
	if(socketServerESI)
		salirConError("Planificador no pudo crear el socket para conectarse con ESI\n");
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

pthread_t crearHiloEjecucion()
{
	pthread_t hilo;
	pthread_create(&hilo, NULL, (void*) ejecucionDeESI, NULL);
	return hilo;
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
