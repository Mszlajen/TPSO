#include "planificador.h"
#include "configuracion.h"
#include "colas.h"

socket_t socketCoord = ERROR, socketServerESI = ERROR;

pthread_mutex_t enPausa;

pthread_mutex_t mReady, mBloqueados, mFinalizados;

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
	char *linea, **palabras;
	while(1)
	{
		linea = readline("Comando:");
		//Aca va a ir el procesamiento para ver si es una instrucción
		//y su procesamiento correspondiente
		palabras = string_split(linea, " ");
		switch(convertirComando(palabras[0]))
		{
		case pausar:
			pthread_mutex_lock(&enPausa);
			break;
		case continuar:
			pthread_mutex_unlock(&enPausa);
			break;
		case bloquear:
			comandoBloquear(palabras);
			break;
		case desbloquear:
			comandoDesbloquear(palabras);
			break;
		case listar:
			comandoListar(palabras);
			break;
		default:
			system(linea);
		}
		free(linea);
		string_iterate_lines(palabras, free);
	}
}

void ejecucionDeESI()
{
	ESI* enEjecucion;
	consultaCoord * consulta = NULL;
	resultado_t resultadoConsulta, *resultadoEjecucion = NULL;
	int8_t mensRecibido;
	while(1)
	{
		pthread_mutex_lock(&enPausa);
		pthread_mutex_lock(&mReady);
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
			salirConError("Se desconecto el coordinador.");
		}
		pthread_mutex_unlock(&mReady);

		enviarEncabezado(enEjecucion -> socket, 7); //Enviar el aviso de ejecucion

		consulta = recibirConsultaCoord();
		resultadoConsulta = procesarConsultaCoord(enEjecucion, consulta);
		enviarRespuestaConsultaCoord(socketCoord, resultadoConsulta);

		mensRecibido = recibirMensaje(enEjecucion -> socket, sizeof(resultado_t), (void**) &resultadoEjecucion);
		if(!mensRecibido || !(*resultadoEjecucion))
			finalizarESI(enEjecucion);
		else
			ejecutarInstruccion(enEjecucion);
		free(resultadoEjecucion);
		pthread_mutex_unlock(&enPausa);
	}
}

void escucharPorESI ()
{
	int socketNuevaESI;
	struct sockaddr_storage infoDirr;
	socklen_t size_infoDirr = sizeof(struct sockaddr_storage);
	ESI* nuevaESI;
	while(1)
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
		{
			pthread_mutex_lock(&mReady);
			listarParaEjecucion(nuevaESI);
			pthread_mutex_unlock(&mReady);
		}
		else
		{
			destruirESI(nuevaESI);
			error_show("Fallo en la conexion de ESI\n");
		}
	}
}

void comandoBloquear(char** palabras)
{
	id_t IDparaBloquear = atoi(palabras[2]);
	ESI* ESIParaBloquear = NULL;
	if(!IDparaBloquear)
	{
		printf("El parametro ID no es valido.\n");
		return;
	}

	pthread_mutex_lock(&mReady);
	if(!(ESIParaBloquear = ESIEnReady(IDparaBloquear)))
	{
		printf("El ESI no se encuentra en un estado bloqueable o no existe.\n");
		return;
	}
	pthread_mutex_unlock(&mReady);

	pthread_mutex_lock(&mBloqueados);
	bloquearESI(ESIParaBloquear, palabras[1]);
	pthread_mutex_unlock(&mBloqueados);
}

void comandoDesbloquear(char** palabras)
{
	pthread_mutex_lock(&mBloqueados);
	ESI* ESIDesbloqueado = desbloquearESIDeClave(palabras[1]);
	if(!ESIDesbloqueado)
	{
		printf("No hay ESI bloqueados para esa clave.\n");
		return;
	}
	pthread_mutex_unlock(&mBloqueados);

	pthread_mutex_lock(&mReady);
	listarParaEjecucion(ESIDesbloqueado);
	pthread_mutex_unlock(&mReady);
}

void comandoListar(char** palabras)
{
	void imprimirID(void* id)
	{
		printf("%i", * (int*) id);
	}
	t_list* listaDeID;

	pthread_mutex_lock(&mBloqueados);
	listaDeID = listarID(palabras[1]);
	if(!listaDeID)
	{
		printf("No hay ESI bloqueados con esa clave.\n");
		return;
	}
	list_iterate(listaDeID, imprimirID);
	pthread_mutex_unlock(&mBloqueados);

	list_destroy_and_destroy_elements(listaDeID, free);
}

void inicializacion ()
{
	if(crearConfiguracion())
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n");
	inicializarColas();
	pthread_mutex_init(&enPausa, NULL);
	pthread_mutex_init(&mReady, NULL);
	pthread_mutex_init(&mBloqueados, NULL);
	pthread_mutex_init(&mFinalizados, NULL);

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

int enviarRespuestaConsultaCoord(socket_t coord, uint8_t respuesta)
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
	int resultado;
	switch(consulta -> instr)
	{
	case get:
		/*
		 * Si un ESI pide una clave que ya tiene tomada
		 * hace un deadlock consigo mismo.
		 */
		pthread_mutex_lock(&mBloqueados);
		if(claveTomada(consulta -> clave))
		{
			pthread_mutex_lock(&mReady);
			bloquearESI(ejecutando, consulta -> clave);
			pthread_mutex_unlock(&mReady);
			resultado = 0;
		}
		else
		{
			reservarClave(ejecutando, consulta -> clave);
			resultado = 1;
		}
		pthread_mutex_unlock(&mBloqueados);
		break;

	case set:
		pthread_mutex_lock(&mBloqueados);
		if(claveTomadaPorESI(consulta -> clave, ejecutando))
		{
			resultado = 1;
		}
		else
		{
			resultado = 0;
		}
		pthread_mutex_unlock(&mBloqueados);
		break;

	case store:
		pthread_mutex_lock(&mBloqueados);
		if(claveTomadaPorESI(consulta -> clave, ejecutando))
		{
			ESI* ESILiberado = liberarClave(consulta -> clave);
			if(ESILiberado)
			{
				pthread_mutex_lock(&mReady);
				listarParaEjecucion(ESILiberado);
				pthread_mutex_unlock(&mReady);
			}
			resultado = 1;
		}
		else
		{
			resultado = 0;
		}
		pthread_mutex_unlock(&mBloqueados);
		break;
	}
	free(consulta);
	return resultado;
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
	if(string_equals_ignore_case(linea, "pausar"))
		return pausar;
	else if(string_equals_ignore_case(linea, "continuar"))
		return continuar;
	else if(string_equals_ignore_case(linea, "bloquear"))
		return bloquear;
	else if(string_equals_ignore_case(linea, "desbloquear"))
		return desbloquear;
	else if(string_equals_ignore_case(linea, "listar"))
		return listar;
	else
		return -1;
}

void liberarRecursos()
{
	eliminarConfiguracion();

	cerrarColas();

	pthread_mutex_destroy(&mReady);
	pthread_mutex_destroy(&mBloqueados);
	pthread_mutex_destroy(&mFinalizados);

	cerrarSocket(socketCoord);
	cerrarSocket(socketServerESI);
}

void salirConError(char * error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}
