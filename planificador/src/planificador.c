#include "planificador.h"
#include "configuracion.h"
#include "colas.h"

sem_t sPausa, sEnEjecucion, contESIEnReady, sSocketCoord;

pthread_mutex_t mReady = PTHREAD_MUTEX_INITIALIZER,
				mBloqueados = PTHREAD_MUTEX_INITIALIZER,
				mFinalizados = PTHREAD_MUTEX_INITIALIZER,
				mEjecutando = PTHREAD_MUTEX_INITIALIZER,
				mLogger = PTHREAD_MUTEX_INITIALIZER;

consultaCoord *ultimaConsulta = NULL;

t_log *logger;

int terminoEjecucion = 0;

int main(int argc, char** argv) {
	inicializacion(argv[1]);

	bloquearClavesConfiguracion();

	socket_t socketNuevasESI = crearServerESI(), socketStatus;
	listen(socketNuevasESI, 5);
	crearHiloCoordinador(socketNuevasESI, &socketStatus);

	crearHiloTerminal(socketStatus);

	crearHiloNuevasESI(socketNuevasESI);

	ESI * esiAEjecutar;
	while(!terminoEjecucion)
	{
		sem_wait(&contESIEnReady);
		sem_wait(&sPausa);
		sem_post(&sPausa);

		sem_wait(&sEnEjecucion);

		if(pthread_mutex_trylock(&mEjecutando))
		{
			sem_post(&sEnEjecucion);
			pthread_mutex_lock(&mEjecutando);
			sem_wait(&sEnEjecucion);
		}

		pthread_mutex_lock(&mReady);
		esiAEjecutar = seleccionarESIPorAlgoritmo(obtenerAlgoritmo());
		pthread_mutex_unlock(&mReady);
		pthread_mutex_unlock(&mEjecutando);

		/*
		 * Este if es porque mientras estaba pausado los ESI en ready
		 * pudieron haberse desconectado o ser bloqueados por consola.
		 */
		if(!esiAEjecutar)
		{
			sem_post(&sEnEjecucion);
			continue;
		}

		if(seDesconectoSocket(esiAEjecutar -> socket))
		{
			finalizarESIBien(esiAEjecutar);
			continue;
		}

		pthread_mutex_lock(&mLogger);
		log_info(logger, "El ESI %i está ejecutando con estimacion %.3f", esiAEjecutar -> id, esiAEjecutar -> estimacion);
		pthread_mutex_unlock(&mLogger);
		ejecucionDeESI(esiAEjecutar);

		sem_post(&sEnEjecucion);
	}

	liberarRecursos();
	exit(0);
}

void terminal(socket_t socketStatus)
{
	char *linea, **palabras;
	booleano enPausa = 0;
	while(!terminoEjecucion)
	{
		linea = readline("");
		if(!linea || !(linea[0])) //Esto es un control para que no se rompa si apretas un enter solo
		{
			free(linea);
			continue;
		}

		pthread_mutex_lock(&mLogger);
		log_info(logger, "[TERMINAL] %s", linea);
		pthread_mutex_unlock(&mLogger);
		palabras = string_split(linea, " ");
		switch(convertirComando(palabras[0]))
		{
		case pausar:
			if(!enPausa)
			{
				sem_wait(&sPausa);
				enPausa = 1;
			}
			break;
		case continuar:
			if(enPausa)
			{
				sem_post(&sPausa);
				enPausa = 0;
			}
			break;
		case bloquear:
			comandoBloquear(palabras[1], palabras[2], enPausa);
			break;
		case desbloquear:
			comandoDesbloquear(palabras[1]);
			break;
		case listar:
			comandoListar(palabras[1]);
			break;
		case kill:
			comandoKill(palabras[1], enPausa);
			break;
		case status:
			comandoStatus(socketStatus, palabras[1]);
			break;
		case deadlock:
			comandoDeadlock();
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
	enviarEncabezado(esi -> socket, 7); //Enviar el aviso de ejecucion
	//printf("Se envio la orden de ejecución.\n");

	header *head = NULL;
	enum resultadoEjecucion *resultado = NULL;
	booleano bloqueo = 0;

	recibirMensaje(esi -> socket, sizeof(header), (void**) &head);
	if(head -> protocolo != 12)
	{ /* ERROR */ }
	free(head);

	recibirMensaje(esi -> socket, sizeof(enum resultadoEjecucion), (void**) &resultado);
	char *stringResultado = resultToString(*resultado);
	pthread_mutex_lock(&mLogger);
	log_info(logger, "ESI %i devolvio %s como resultado", esi -> id, stringResultado);
	pthread_mutex_unlock(&mLogger);
	free(stringResultado);

	//Controlo que el ESI haya terminado correctamente la instrucción.
	if(*resultado > fallos)
	{
		pthread_mutex_lock(&mLogger);
		log_error(logger, "ESI %i fallo durante su ejecución", esi -> id);
		pthread_mutex_unlock(&mLogger);
		free(ultimaConsulta -> clave);
		free(ultimaConsulta);
		free(resultado);
		ultimaConsulta = NULL;
		finalizarESIMal(esi);
		return;
	}

	//Actualizo el estado del sistema en base a la instrucción.
	switch(ultimaConsulta -> tipo)
	{
		case liberadora:
			pthread_mutex_lock(&mBloqueados);
			pthread_mutex_lock(&mReady);
			pthread_mutex_lock(&mLogger);
			log_info(logger, "ESI %i libero %s", esi -> id, ultimaConsulta -> clave);
			pthread_mutex_unlock(&mLogger);
			ESI* esiLiberado = liberarClave(ultimaConsulta -> clave);
			if(esiLiberado)
			{
				recalcularEstimacion(esiLiberado, obtenerAlfa());
				listarParaEjecucion(esiLiberado);
				sem_post(&contESIEnReady);
				pthread_mutex_lock(&mLogger);
				log_info(logger, "ESI %i se agrego a ready con estimacion %.3f", esiLiberado -> id, esiLiberado -> estimacion);
				pthread_mutex_unlock(&mLogger);
			}
			pthread_mutex_unlock(&mBloqueados);
			pthread_mutex_unlock(&mReady);
			break;
		case bloqueante:
			//En realidad acá están mal usados los semaforos porque no están pegados a la region critica.
			pthread_mutex_lock(&mBloqueados);
			if(ultimaConsulta -> resultado)
			{
				quitarESIEjecutando();
				colocarEnColaESI(esi, ultimaConsulta -> clave);
				bloqueo = 1;
				pthread_mutex_lock(&mLogger);
				log_info(logger, "ESI %i se bloqueo por %s", esi -> id, ultimaConsulta -> clave);
				pthread_mutex_unlock(&mLogger);
			}
			else
			{
				reservarClave(esi, ultimaConsulta -> clave);
				pthread_mutex_lock(&mLogger);
				log_info(logger, "ESI %i tomo posesión de %s", esi -> id, ultimaConsulta -> clave);
				pthread_mutex_unlock(&mLogger);
			}
			pthread_mutex_unlock(&mBloqueados);
			break;
		case noDefinido:
			break;
	}

	free(ultimaConsulta -> clave);
	free(ultimaConsulta);
	ultimaConsulta = NULL;

	//Compruebo si el ESI termino su ejecución.
	if(*resultado == fin)
	{
		free(resultado);
		pthread_mutex_lock(&mLogger);
		log_info(logger, "ESI %i termino su ejecución", esi -> id);
		pthread_mutex_unlock(&mLogger);
		finalizarESIBien(esi);
		return;
	}

	free(resultado);

	actualizarEstimacion(esi);
	if(!bloqueo)
		sem_post(&contESIEnReady);
}

void escucharPorESI (socket_t socketServerESI)
{
	socket_t socketNuevaESI;
	ESI* nuevaESI;
	header* head;

	listen(socketServerESI, 5);
	while(!terminoEjecucion)
	{
		socketNuevaESI = ERROR;
		socketNuevaESI = aceptarConexion(socketServerESI);
		if(socketNuevaESI == ERROR)
		{
			continue;
		}

		recibirMensaje(socketNuevaESI, sizeof(header), (void**) &head);
		if(head -> protocolo != 3)
		{
			free(head);
			continue;
		}
		free(head);

		nuevaESI = crearESI(socketNuevaESI, obtenerEstimacionInicial());
		if(!enviarIdESI(socketNuevaESI, nuevaESI -> id))
		{
			pthread_mutex_lock(&mLogger);
			log_info(logger, "ESI %i se conecto", nuevaESI -> id);
			pthread_mutex_unlock(&mLogger);
			pthread_mutex_lock(&mReady);
			listarParaEjecucion(nuevaESI);
			pthread_mutex_unlock(&mReady);
			sem_post(&contESIEnReady);
		}
		else
		{
			destruirESI(nuevaESI);
		}
	}
}

void comunicacionCoord(socket_t socketCoord)
{
	header *head;
	while(!terminoEjecucion)
	{
		if(recibirMensaje(socketCoord, sizeof(header), (void**) &head))
		{
			salirConError("Se desconecto el coordinador");
			free(head);
			continue;
		}

		//Consulta estado clave
		ultimaConsulta = recibirConsultaCoord(socketCoord);
		if(ultimaConsulta -> tipo == bloqueante)
			ultimaConsulta -> resultado = claveTomada(ultimaConsulta -> clave);
		else
			ultimaConsulta -> resultado = claveTomadaPorESI(ultimaConsulta -> clave, ultimaConsulta -> id_esi);
		enviarRespuestaConsultaCoord(socketCoord, ultimaConsulta -> resultado);
		free(head);
	}
}

void comandoBloquear(char* clave, char* IdESI, booleano enPausa)
{
	ESI_id IDparaBloquear = atoi(IdESI);
	ESI* ESIParaBloquear = NULL;
	booleano esEjecucion = 0;

	//Compruebo que parametro ID haya sido un posible ID
	if(IDparaBloquear <= 0)
	{
		pthread_mutex_lock(&mLogger);
		log_error(logger, "[TERMINAL] Parametro ID invalido");
		pthread_mutex_unlock(&mLogger);
		return;
	}

	//Compruebo que sea el ESI ejecutando, espero a que termine la ejecución
	//y despues compruebo que lo siga siendo
	pthread_mutex_lock(&mEjecutando);
	if(esESIEnEjecucion(IDparaBloquear) && !enPausa)
	{
		printf("[TERMINAL] El ESI a bloquear está ejecutando, esperando fin de ejecución.\n");
		sem_wait(&sEnEjecucion);
		esEjecucion = 1;
	}
	pthread_mutex_lock(&mBloqueados);
	pthread_mutex_lock(&mReady);
	if(esESIEnEjecucion(IDparaBloquear))
	{
		ESIParaBloquear = ESIEjecutando();
		if(claveTomada(clave))
		{
			bloquearESI(ESIParaBloquear, clave);
			pthread_mutex_lock(&mLogger);
			log_info(logger, "[TERMINAL] ESI %i se bloqueo por %s", ESIParaBloquear -> id, clave);
			pthread_mutex_unlock(&mLogger);
		}
		else
		{
			reservarClave(ESIParaBloquear, clave);
			pthread_mutex_lock(&mLogger);
			log_info(logger, "[TERMINAL] ESI %i tomo posesion de %s", ESIParaBloquear -> id, clave);
			pthread_mutex_unlock(&mLogger);
		}
	}
	else if((ESIParaBloquear = ESIEnReady(IDparaBloquear)))
	{
		sem_wait(&contESIEnReady);
		if(claveTomada(clave))
		{
			bloquearESI(ESIParaBloquear, clave);
			pthread_mutex_lock(&mLogger);
			log_info(logger, "[TERMINAL] ESI %i se bloqueo por %s", ESIParaBloquear -> id, clave);
			pthread_mutex_unlock(&mLogger);
		}
		else
		{
			reservarClave(ESIParaBloquear, clave);
			pthread_mutex_lock(&mLogger);
			log_info(logger, "[TERMINAL] ESI %i tomo posesion de %s", ESIParaBloquear -> id, clave);
			pthread_mutex_unlock(&mLogger);
		}
	}
	else if(estaEnFinalizados(IDparaBloquear))
	{
		pthread_mutex_lock(&mLogger);
		log_error(logger, "[TERMINAL] ESI %i está finalizado", IDparaBloquear);
		pthread_mutex_unlock(&mLogger);
	}
	else if((ESIParaBloquear = ESIEstaBloqueado(IDparaBloquear)))
	{
		if(claveTomada(clave))
		{
			pthread_mutex_lock(&mLogger);
			log_error(logger, "[TERMINAL] ESI %i bloqueado; %s tomada", IDparaBloquear, clave);
			pthread_mutex_unlock(&mLogger);
		}
		else
		{
			reservarClave(ESIParaBloquear, clave);
			pthread_mutex_lock(&mLogger);
			log_info(logger, "[TERMINAL] ESI %i tomo posesion de %s", ESIParaBloquear -> id, clave);
			pthread_mutex_unlock(&mLogger);
		}
	}
	else
	{
		pthread_mutex_lock(&mLogger);
		log_error(logger, "[TERMINAL] ESI %i no existe", IDparaBloquear);
		pthread_mutex_unlock(&mLogger);
	}
	pthread_mutex_unlock(&mReady);
	pthread_mutex_unlock(&mBloqueados);
	pthread_mutex_unlock(&mEjecutando);

	if(esEjecucion)
		sem_post(&sEnEjecucion);
}

void comandoDesbloquear(char* clave)
{
	pthread_mutex_lock(&mBloqueados);
	ESI* ESIDesbloqueado = desbloquearESIDeClave(clave);
	if(!ESIDesbloqueado)
	{
		if(claveTomada(clave))
		{
			liberarClave(clave); //No necesito leer el valor de retorno porque sé que es NULL
			pthread_mutex_lock(&mLogger);
			log_info(logger, "[TERMINAL] %s liberada", clave);
			pthread_mutex_unlock(&mLogger);
		}
		else
		{
			pthread_mutex_lock(&mLogger);
			log_error(logger, "[TERMINAL] %s no está tomada ni tiene ESI bloqueados", clave);
			pthread_mutex_unlock(&mLogger);
		}
		pthread_mutex_unlock(&mBloqueados);
		return;
	}
	pthread_mutex_unlock(&mBloqueados);

	pthread_mutex_lock(&mReady);
	recalcularEstimacion(ESIDesbloqueado, obtenerAlfa());
	listarParaEjecucion(ESIDesbloqueado);
	pthread_mutex_lock(&mLogger);
	log_info(logger, "[TERMINAL] ESI %i se agrego a ready con estimacion %.3f", ESIDesbloqueado -> id, ESIDesbloqueado -> estimacion);
	pthread_mutex_unlock(&mLogger);
	pthread_mutex_unlock(&mReady);
	sem_post(&contESIEnReady);
}

void comandoListar(char* clave)
{
	char *lista = string_new();
	void imprimirID(void* id)
	{
		string_append_with_format(&lista, "%i ", *((ESI_id*) id));
	}
	t_list* listaDeID;

	pthread_mutex_lock(&mBloqueados);
	listaDeID = listarID(clave);
	pthread_mutex_unlock(&mBloqueados);

	if(!listaDeID)
	{
		printf("[TERMINAL] %s no tiene ESI bloqueados", clave);
	}
	else
	{
		list_iterate(listaDeID, imprimirID);
		list_destroy_and_destroy_elements(listaDeID, free);
		printf("[TERMINAL] %s tiene bloqueados: %s", clave, lista);
	}
	free(lista);
}

void comandoKill(char *IdESI, booleano enPausa)
{
	ESI_id IDparaMatar = atoi(IdESI);
	ESI* ESIParaMatar = NULL;
	booleano esEjecutando = 0;

	//Compruebo que parametro ID haya sido un posible ID
	if(IDparaMatar <= 0)
	{
		pthread_mutex_lock(&mLogger);
		log_error(logger, "[TERMINAL] Parametro ID invalido");
		pthread_mutex_unlock(&mLogger);
		return;
	}

	//Compruebo que sea el ESI ejecutando, espero a que termine la ejecución
	//y despues compruebo que lo siga siendo
	pthread_mutex_lock(&mEjecutando);
	if(esESIEnEjecucion(IDparaMatar) && !enPausa)
	{
		printf("[TERMINAL] El ESI a matar está ejecutando, esperando fin de ejecución.\n");
		sem_wait(&sEnEjecucion);
		esEjecutando = 1;
	}

	if(esESIEnEjecucion(IDparaMatar))
	{
		ESIParaMatar = ESIEjecutando();
		finalizarESIBien(ESIParaMatar);
		pthread_mutex_lock(&mLogger);
		log_info(logger, "[TERMINAL] ESI %s fue finalizado", IdESI);
		pthread_mutex_unlock(&mLogger);
	}
	else if((ESIParaMatar = ESIEnReady(IDparaMatar)))
	{
		sem_wait(&contESIEnReady);
		finalizarESIBien(ESIParaMatar);
		pthread_mutex_lock(&mLogger);
		log_info(logger, "[TERMINAL] ESI %s fue finalizado", IdESI);
		pthread_mutex_unlock(&mLogger);
	}
	else if((ESIParaMatar = ESIEstaBloqueado(IDparaMatar)))
	{
		finalizarESIBien(ESIParaMatar);
		pthread_mutex_lock(&mLogger);
		log_info(logger, "[TERMINAL] ESI %s fue finalizado", IdESI);
		pthread_mutex_unlock(&mLogger);
	}
	else if(estaEnFinalizados(IDparaMatar))
	{
		pthread_mutex_lock(&mLogger);
		log_error(logger, "[TERMINAL] ESI %s está finalizado", IdESI);
		pthread_mutex_unlock(&mLogger);
	}
	else
	{
		pthread_mutex_lock(&mLogger);
		log_error(logger, "[TERMINAL] ESI %s no existe", IdESI);
		pthread_mutex_unlock(&mLogger);
	}
	pthread_mutex_unlock(&mEjecutando);

	if(esEjecutando)
		sem_post(&sEnEjecucion);
}

void comandoStatus(socket_t socketStatus, char* clave)
{
	printf("[TERMINAL] Preguntando al coordinador por el estado de la clave %s\n", clave);

	consultaStatus consStatus;
	consStatus.clave = clave;
	consStatus.tamClave = string_length(clave) + 1;

	enviarEncabezado(socketStatus, 13);
	enviarBuffer(socketStatus, (void*) &(consStatus.tamClave), sizeof(tamClave_t));
	enviarBuffer(socketStatus, (void*) consStatus.clave, consStatus.tamClave);

	recibirRespuestaStatus(socketStatus, &consStatus);

	ESI *dueno;
	if(claveTomadaPor(clave, &dueno))
	{
		printf("[TERMINAL] La clave %s se encuentra tomada por el ", clave);
		if(dueno)
			printf("ESI %i", dueno -> id);
		else
			printf("sistema");
		printf(".\n");
	}
	else
		printf("[TERMINAL] La clave %s no se encuentra tomada por nadie.\n", clave);

	switch(consStatus.estado)
	{
	case existente:
		printf("[TERMINAL] La clave %s se encuentra en la instancia %i (%s) y su valor actual %s.\n", clave,
				consStatus.id, consStatus.nombre, consStatus.valor);
		free(consStatus.valor);
		break;
	case innexistente:
		printf("[TERMINAL] La clave %s no existe en el coordinador, actualmente iria a la instancia %i (%s).\n", clave,
				consStatus.id, consStatus.nombre);
		break;
	case sinIniciar:
		printf("[TERMINAL] La clave %s no tiene instacia asignada, actualmente iria a la instancia %i (%s).\n", clave,
				consStatus.id, consStatus.nombre);
		break;
	case caida:
		printf("[TERMINAL] La clave %s se encuentra en la instancia %i (%s) que está desconectada.\n", clave,
				consStatus.id, consStatus.nombre);
		break;
	case sinValor:
		printf("[TERMINAL] La clave %s se encontraba en la instancia %i (%s) pero fue reemplazada.\n", clave,
				consStatus.id, consStatus.nombre);
		break;
	}
	comandoListar(clave);
	free(consStatus.nombre);
}

void comandoDeadlock()
{
	char *lista = string_new();
	void imprimirID(void* id)
	{
		string_append_with_format(&lista, "%i ", *((ESI_id*) id));
	}
	t_list* esiEnDeadlock = NULL;
	printf("[TERMINAL] Analizando existencia de deadlock...\n");
	pthread_mutex_lock(&mBloqueados);
	esiEnDeadlock = detectarDeadlock();
	pthread_mutex_unlock(&mBloqueados);
	if(!esiEnDeadlock)
		printf("[TERMINAL] No hay deadlock");
	else
	{
		list_iterate(esiEnDeadlock, imprimirID);
		printf("[TERMINAL] Existe deadlock. ESI involucrados: %s\n", lista);
		list_destroy_and_destroy_elements(esiEnDeadlock, free);
	}
	free(lista);
}

void inicializacion (char* dirConfig)
{
	logger = log_create("log de planificación", "planificador", true, LOG_LEVEL_INFO);

	if(crearConfiguracion(dirConfig))
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n");
	inicializarColas();

	//El tercer parametro es el valor inicial del semaforo
	sem_init(&sPausa, 0, 1);
	sem_init(&contESIEnReady, 0, 0);
	sem_init(&sEnEjecucion, 0, 1);

}

void bloquearClavesConfiguracion()
{
	char *clavesABloquear = obtenerBloqueosConfiguracion();
	if(clavesABloquear)
	{
		char** claves = string_split(clavesABloquear, ",");
		string_iterate_lines(claves, reservarClaveSinESI);
		string_iterate_lines(claves, free);
	}
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

booleano enviarIdESI(socket_t sock, ESI_id id)
{
	return enviarEncabezado(sock, 6) || enviarBuffer(sock, (void*) &id, sizeof(id));
}

int enviarRespuestaConsultaCoord(socket_t coord, booleano respuesta)
{
	return enviarEncabezado(coord, 10) || enviarBuffer(coord, &respuesta, sizeof(booleano));
}

consultaCoord* recibirConsultaCoord(socket_t socketCoord)
{
	consultaCoord *consulta = malloc(sizeof(consultaCoord));

	ESI_id *id_esi;
	recibirMensaje(socketCoord, sizeof(ESI_id), (void**) &id_esi);
	consulta -> id_esi = *id_esi;
	free(id_esi);

	enum tipoDeInstruccion *tipo;
	recibirMensaje(socketCoord, sizeof(enum tipoDeInstruccion), (void**) &tipo);
	consulta -> tipo = *tipo;
	free(tipo);

	tamClave_t *tamClave;
	recibirMensaje(socketCoord, sizeof(tamClave_t), (void**) &tamClave);
	consulta -> tamClave = *tamClave;
	free(tamClave);

	recibirMensaje(socketCoord, consulta -> tamClave, (void**) &consulta -> clave);

	return consulta;
}

void recibirRespuestaStatus(socket_t socketCoord, consultaStatus* status)
{
	void *buffer = NULL;
	recibirMensaje(socketCoord, sizeof(header), &buffer);
	if(((header*) buffer) -> protocolo != 14)
	{ /*ERROR*/}
	free(buffer);

	recibirMensaje(socketCoord, sizeof(instancia_id) + sizeof(tamNombreInstancia_t), &buffer);
	status -> id = *((instancia_id*) buffer);
	status -> tamNombre = *((tamNombreInstancia_t*) (buffer + sizeof(instancia_id)));
	free(buffer);

	recibirMensaje(socketCoord, status -> tamNombre, &buffer);
	status -> nombre = string_duplicate((char*) buffer);
	free(buffer);

	recibirMensaje(socketCoord, sizeof(enum estadoClave), &buffer);
	status -> estado = *((enum estadoClave*) buffer);
	free(buffer);

	if(status -> estado == existente)
	{
		recibirMensaje(socketCoord, sizeof(tamValor_t), &buffer);
		status -> tamValor = *((tamValor_t*) buffer);
		free(buffer);

		recibirMensaje(socketCoord, status -> tamValor, &buffer);
		status -> valor = string_duplicate((char*) buffer);
		free(buffer);
	}
}

socket_t crearServerESI ()
{
	char * puerto = obtenerPuerto();
	char * IPConexion = obtenerIP();
	socket_t socketServerESI = crearSocketServer(IPConexion, puerto);
	if(socketServerESI == ERROR)
		salirConError("Planificador no pudo crear el socket para conectarse con ESI\n");
	return socketServerESI;
}

/*
 * Las siguientes funciones repiten codigo pero
 * no creo que sea buen momento para aprender
 * a crear funciones de orden superior en C
 */
pthread_t crearHiloTerminal(socket_t socketStatus)
{
	pthread_t hilo;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, &attr, (void*) terminal, (void*) socketStatus);
	pthread_attr_destroy(&attr);
	return hilo;
}

pthread_t crearHiloNuevasESI(socket_t socketNuevosESI)
{
	pthread_t hilo;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, &attr, (void*) escucharPorESI, (void*) socketNuevosESI);
	pthread_attr_destroy(&attr);
	return hilo;
}

pthread_t crearHiloCoordinador(socket_t socketEscucha, socket_t* socketStatus)
{
	socket_t socketCoord = ERROR;
	pthread_t hilo;
	pthread_attr_t attr;

	socketCoord = conectarConCoordinador();

	tamClave_t tamIP = string_length(obtenerIP()) + sizeof(char), tamPuerto = string_length(obtenerPuerto()) + sizeof(char);

	if(socketCoord == ERROR)
		salirConError("No se pudo conectar con el coordinador.\n");
	if(enviarEncabezado(socketCoord, 1) || enviarBuffer(socketCoord, &tamIP, sizeof(tamClave_t)) || enviarBuffer(socketCoord, obtenerIP(), tamIP) ||
		enviarBuffer(socketCoord, &tamPuerto, sizeof(tamClave_t)) || enviarBuffer(socketCoord, obtenerPuerto(), tamPuerto)) //Enviar el handshake
		salirConError("Fallo al enviar el handshake planificador -> Coordinador\n");

	*socketStatus = aceptarConexion(socketEscucha);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, &attr, (void*) comunicacionCoord, (void*) socketCoord);
	pthread_attr_destroy(&attr);

	return hilo;
}

enum comandos convertirComando(char* linea)
{
	if(string_equals_ignore_case(linea, "pausar") || string_equals_ignore_case(linea, "p"))
		return pausar;
	else if(string_equals_ignore_case(linea, "continuar"))
		return continuar;
	else if(string_equals_ignore_case(linea, "bloquear"))
		return bloquear;
	else if(string_equals_ignore_case(linea, "desbloquear"))
		return desbloquear;
	else if(string_equals_ignore_case(linea, "listar"))
		return listar;
	else if(string_equals_ignore_case(linea, "kill"))
		return kill;
	else if(string_equals_ignore_case(linea, "status"))
		return status;
	else if(string_equals_ignore_case(linea, "deadlock"))
		return deadlock;
	else
		return -1;
}

int max (int v1, int v2)
{
	return v1 > v2? v1 : v2;
}

void finalizarESIBien(ESI* esi)
{
	ESI* desbloqueado = NULL;

	pthread_mutex_lock(&mBloqueados);
	while(!list_is_empty(esi -> recursos))
	{
		desbloqueado = liberarClave(list_get(esi -> recursos, 0));
		if(desbloqueado)
		{
			listarParaEjecucion(desbloqueado);
			sem_post(&contESIEnReady);
		}
	}
	pthread_mutex_lock(&mReady);
	pthread_mutex_lock(&mFinalizados);
	finalizarESI(esi);
	pthread_mutex_unlock(&mFinalizados);
	pthread_mutex_unlock(&mReady);
	pthread_mutex_unlock(&mBloqueados);
}

void finalizarESIMal(ESI* esi)
{
	pthread_mutex_lock(&mBloqueados);
	entregarRecursosAlSistema(esi);
	pthread_mutex_lock(&mReady);
	pthread_mutex_lock(&mFinalizados);
	finalizarESI(esi);
	pthread_mutex_unlock(&mFinalizados);
	pthread_mutex_unlock(&mReady);
	pthread_mutex_unlock(&mBloqueados);
}

void liberarRecursos()
{
	eliminarConfiguracion();

	cerrarColas();

	pthread_mutex_destroy(&mReady);
	pthread_mutex_destroy(&mBloqueados);
	pthread_mutex_destroy(&mFinalizados);
	pthread_mutex_destroy(&mEjecutando);
	pthread_mutex_destroy(&mLogger);
	sem_destroy(&sPausa);
	sem_destroy(&contESIEnReady);

	log_destroy(logger);

}

void salirConError(char * error)
{
	pthread_mutex_lock(&mLogger);
	log_error(logger, error);
	pthread_mutex_unlock(&mLogger);
	terminoEjecucion = 1;
}
