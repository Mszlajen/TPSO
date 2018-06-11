#include "planificador.h"
#include "configuracion.h"
#include "colas.h"

pthread_mutex_t enPausa, ejecutando;

pthread_mutex_t mReady, mBloqueados, mFinalizados, mESIEjecutando;

socket_t socketLlegadaNuevaESI = ERROR, socketEjecucionDeCoordinador = ERROR;

consultaCoord *ultimaConsulta = NULL;

int terminoEjecucion = 0;

int main(int argc, char** argv) {
	inicializacion(argv[1]);

	bloquearClavesConfiguracion();

	crearHiloCoordinador();

	crearHiloTerminal();

	crearHiloNuevasESI();

	ESI * esiAEjecutar;
	while(!terminoEjecucion)
	{
		pthread_mutex_lock(&enPausa);
		while(readyVacio());

		pthread_mutex_lock(&mESIEjecutando);
		pthread_mutex_lock(&mReady);
		esiAEjecutar = seleccionarESIPorAlgoritmo(obtenerAlgoritmo());
		pthread_mutex_unlock(&mReady);

		pthread_cancel(esiAEjecutar -> hiloDeFin);
		ejecucionDeESI(esiAEjecutar);

		crearFinEjecucion(esiAEjecutar);

		pthread_mutex_lock(&mESIEjecutando);
		pthread_mutex_unlock(&enPausa);
	}

	liberarRecursos();
	exit(0);
}

void terminal()
{
	char *linea, **palabras;
	while(!terminoEjecucion)
	{
		linea = readline("Comando:");
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

void ejecucionDeESI(ESI* esi)
{
	enum resultadoEjecucion res;
	void* resultado = NULL;

	enviarEncabezado(esi -> socket, 7); //Enviar el aviso de ejecucion

	recibirMensaje(esi -> socket, sizeof(header) + sizeof(enum resultadoEjecucion), resultado);

	if(((header*) resultado) -> protocolo != 12)
	{ /* ERROR */ }

	res = *((enum resultadoEjecucion*) resultado + sizeof(header));

	free(resultado);

	if(res == fallo || res == fin)
	{
		printf("Se termino el esi %i", esi -> id);
		pthread_mutex_lock(&mBloqueados);
		pthread_mutex_lock(&mReady);
		finalizarESI(esi);
		pthread_mutex_unlock(&mBloqueados);
		pthread_mutex_unlock(&mReady);
	}

	switch(ultimaConsulta -> tipo)
	{
	case liberadora:
		pthread_mutex_lock(&mBloqueados);
		pthread_mutex_lock(&mReady);
		listarParaEjecucion(liberarClave(ultimaConsulta -> clave));
		pthread_mutex_unlock(&mBloqueados);
		pthread_mutex_unlock(&mReady);
		break;
	case bloqueante:
		pthread_mutex_lock(&mBloqueados);
		if(ultimaConsulta -> resultado)
			colocarEnColaESI(esi, ultimaConsulta -> clave);
		else
			reservarClave(esi, ultimaConsulta -> clave);
		pthread_mutex_unlock(&mBloqueados);
		break;
	default:
		break;
	}

	free(ultimaConsulta -> clave);
	free(ultimaConsulta);
	ultimaConsulta = NULL;
}

void escucharPorESI ()
{
	socket_t socketServerESI = ERROR, socketNuevaESI;
	ESI* nuevaESI;

	socketServerESI = crearServerESI();
	while(!terminoEjecucion)
	{
		socketNuevaESI = ERROR;
		listen(socketServerESI, 5);
		socketNuevaESI = aceptarConexion(socketServerESI);
		if(socketNuevaESI == ERROR)
		{
			error_show("Fallo en la conexion de ESI\n");
			continue;
		}
		nuevaESI = crearESI(socketNuevaESI, obtenerEstimacionInicial());
		if(!enviarIdESI(socketNuevaESI, nuevaESI -> id))
		{
			crearFinEjecucion(nuevaESI);
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

void escucharPorFinESI(ESI* esi)
{
	while(1)
	{
		listen(esi -> socket, 5);
		if(seDesconectoSocket(esi -> socket))
		{
			printf("Se termino el esi %i", esi -> id);
			pthread_mutex_lock(&mESIEjecutando);
			pthread_mutex_lock(&mBloqueados);
			pthread_mutex_lock(&mReady);
			finalizarESI(esi);
			pthread_mutex_unlock(&mBloqueados);
			pthread_mutex_unlock(&mReady);
			pthread_mutex_unlock(&mESIEjecutando);
			pthread_exit(NULL);
		}
	}
}

void comunicacionCoord(socket_t socketCoord)
{
	int maxFd = max(socketCoord, socketEjecucionDeCoordinador);
	fd_set master, read;

	FD_ZERO(&master);
	FD_SET(socketCoord, &master);
	FD_SET(socketEjecucionDeCoordinador, &master);
	while(1)
	{
		read = master;
		select(maxFd, &read, NULL, NULL, NULL);

		if(FD_ISSET(socketCoord, &read))
		{
			if(seDesconectoSocket(socketCoord))
				salirConError("Se desconecto el coordinador.");
			//Consulta estado clave
			ultimaConsulta = recibirConsultaCoord(socketCoord);
			enviarRespuestaConsultaCoord(socketCoord,
						claveTomadaPorESI(ultimaConsulta -> clave, ultimaConsulta -> id_esi));
		}
		else
		{
			//status
		}
	}
}

void comandoBloquear(char** palabras)
{
	ESI_id IDparaBloquear = atoi(palabras[2]);
	ESI* ESIParaBloquear = NULL;
	uint8_t esESIEjecutando;
	if(!IDparaBloquear)
	{
		printf("El parametro ID no es valido.\n");
		return;
	}

	if(esESIEnEjecucion(IDparaBloquear))
	{
		pthread_mutex_lock(&mESIEjecutando);
		/*
		 * Si el ESI a bloquear es el que se está ejecutando pueden pasar dos cosas:
		 * 1- No está ejecutando ninguna instrucción por lo tanto el mutex es tomado
		 * inmediatamente y el if siguiente es innecesario.
		 * 2- Está ejecutando una instrucción por lo tanto el mutex bloquea el hilo
		 * y en el intermedio hasta su liberación el ESI pudo haber terminado su ejecución
		 * o bloquearse y hay que comprobar que siga siendo el que ejecuta.
		 */
		if((esESIEjecutando = esESIEnEjecucion(IDparaBloquear)))
			ESIParaBloquear = ESIEjecutando();
		else
			pthread_mutex_unlock(&mESIEjecutando);
	}
	pthread_mutex_lock(&mBloqueados);
	pthread_mutex_lock(&mReady);
	if(!esESIEjecutando && !(ESIParaBloquear = ESIEnReady(IDparaBloquear)))
	{
		printf("El ESI no se encuentra en ready, ejecutando o no existe.\n");
		pthread_mutex_unlock(&mReady);
		pthread_mutex_unlock(&mBloqueados);
		return;
	}


	bloquearESI(ESIParaBloquear, palabras[1]);

	pthread_mutex_unlock(&mReady);

	pthread_mutex_unlock(&mBloqueados);

	if(esESIEjecutando)
		pthread_mutex_unlock(&mESIEjecutando);

}

void comandoDesbloquear(char** palabras)
{
	/*
	 * Si la clave que se le pasa es inexistente, el comando no hace
	 * nada y tampoco avisa al usuario.
	 * (Revisar, tal vez)
	 */
	pthread_mutex_lock(&mBloqueados);
	ESI* ESIDesbloqueado = desbloquearESIDeClave(palabras[1]);
	if(!ESIDesbloqueado)
	{
		printf("No hay ESI bloqueados para esa clave\n");
		liberarClave(palabras[1]); //No necesito leer el valor de retorno porque sé que es NULL
		pthread_mutex_unlock(&mBloqueados);
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
		printf("%i", * (ESI_id*) id);
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

void inicializacion (char* dirConfig)
{
	if(crearConfiguracion(dirConfig))
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n");
	inicializarColas();
	pthread_mutex_init(&enPausa, NULL);
	pthread_mutex_init(&ejecutando, NULL);
	pthread_mutex_init(&mReady, NULL);
	pthread_mutex_init(&mBloqueados, NULL);
	pthread_mutex_init(&mFinalizados, NULL);
	pthread_mutex_init(&mESIEjecutando, NULL);

	/*
	 * Crea los socket en cualquier IP y puerto que el SO
	 * tenga disponible.
	 */
	socketLlegadaNuevaESI = crearSocketServer(NULL, "0");
	socketEjecucionDeCoordinador = crearSocketServer(NULL, "0");

}

void bloquearClavesConfiguracion()
{
	char** claves = string_split(obtenerBloqueosConfiguracion(), ",");
	string_iterate_lines(claves, reservarClaveSinESI);
	string_iterate_lines(claves, free);
}

socket_t conectarConCoordinador()
{
	char * IP = obtenerDireccionCoordinador();
	char * puerto = obtenerPuertoCoordinador();
	return crearSocketCliente(IP, puerto);
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
	int tamEncabezado = sizeof(header), tamId = sizeof(ESI_id);
	void* buffer = malloc(tamEncabezado + tamId);
	memcpy(buffer, &respuesta, tamEncabezado);
	memcpy(buffer + tamEncabezado, &id, tamId);
	int resultado = enviarBuffer(sock, buffer, sizeof(buffer));
	free(buffer);
	return resultado;
}

int enviarRespuestaConsultaCoord(socket_t coord, booleano respuesta)
{
	header encabezado;
	encabezado.protocolo = 10;
	void* buffer = malloc(sizeof(header) + sizeof(respuesta));
	memcpy(buffer, &encabezado, sizeof(header));
	memcpy(buffer + sizeof(header), &respuesta, sizeof(respuesta));
	return enviarBuffer(coord, buffer, sizeof(buffer));
}

consultaCoord* recibirConsultaCoord(socket_t socketCoord)
{
	consultaCoord *consulta = malloc(sizeof(consultaCoord));

	ESI_id *id_esi;
	recibirMensaje(socketCoord, sizeof(enum tipoDeInstruccion), (void**) &id_esi);
	consulta -> tipo = *id_esi;
	free(id_esi);

	enum tipoDeInstruccion *tipo;
	recibirMensaje(socketCoord, sizeof(enum tipoDeInstruccion), (void**) &tipo);
	consulta -> tipo = *tipo;
	free(tipo);

	uint8_t *tamClave;
	recibirMensaje(socketCoord, sizeof(uint8_t), (void**) &tamClave);
	consulta -> tamClave = *tamClave;
	free(tamClave);

	recibirMensaje(socketCoord, consulta -> tamClave, (void**) &consulta -> clave);

	return consulta;
}

socket_t crearServerESI ()
{
	char * puerto = obtenerPuerto();
	char * IPConexion = obtenerIP();
	socket_t socketServerESI = crearSocketServer(IPConexion, puerto);
	if(socketServerESI)
		salirConError("Planificador no pudo crear el socket para conectarse con ESI\n");
	return socketServerESI;
}

/*
 * Las siguientes funciones repiten codigo pero
 * no creo que sea buen momento para aprender
 * a crear funciones de orden superior en C
 */
pthread_t crearHiloTerminal()
{
	pthread_t hilo;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, &attr, (void*) terminal, NULL);
	pthread_attr_destroy(&attr);
	return hilo;
}

pthread_t crearHiloNuevasESI()
{
	pthread_t hilo;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, &attr, (void*) escucharPorESI, NULL);
	pthread_attr_destroy(&attr);
	return hilo;
}

pthread_t crearFinEjecucion(ESI* esi)
{
	pthread_t hilo;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, NULL, (void*) escucharPorFinESI, (void*) esi);
	pthread_attr_destroy(&attr);
	esi -> hiloDeFin = hilo;
	return hilo;
}

pthread_t crearHiloCoordinador()
{
	socket_t socketCoord = ERROR;
	pthread_t hilo;
	pthread_attr_t attr;

	socketCoord = conectarConCoordinador();

	if(socketCoord == ERROR)
		salirConError("No se pudo conectar con el coordinador.");
	if(!enviarEncabezado(socketCoord, 1)) //Enviar el handshake
		salirConError("Fallo al enviar el handshake planificador -> Coordinador\n");

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, NULL, (void*) comunicacionCoord, (void*) socketCoord);
	pthread_attr_destroy(&attr);

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

int max (int v1, int v2)
{
	if(v1 > v2)
		return v1;
	else
		return v2;
}

/*
 * Falta sincronizar el cierre de la colas y
 * liberar los hilos.
 */
void liberarRecursos()
{
	eliminarConfiguracion();

	cerrarColas();

	pthread_mutex_destroy(&mReady);
	pthread_mutex_destroy(&mBloqueados);
	pthread_mutex_destroy(&mFinalizados);
	pthread_mutex_destroy(&mESIEjecutando);
	pthread_mutex_destroy(&enPausa);

}

void salirConError(char * error)
{
	error_show(error);
	terminoEjecucion = 1;
}
