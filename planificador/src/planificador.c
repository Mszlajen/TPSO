#include "planificador.h"
#include "configuracion.h"
#include "colas.h"

sem_t sPausa, contESIEnReady, sSocketCoord;

pthread_mutex_t mReady = PTHREAD_MUTEX_INITIALIZER,
				mBloqueados = PTHREAD_MUTEX_INITIALIZER,
				mFinalizados = PTHREAD_MUTEX_INITIALIZER,
				mEjecutando = PTHREAD_MUTEX_INITIALIZER,
				mEnEjecucion = PTHREAD_MUTEX_INITIALIZER,
				mLogger = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cFinEjecucion = PTHREAD_COND_INITIALIZER;

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

		pthread_mutex_lock(&mEjecutando);
		pthread_mutex_lock(&mReady);
		esiAEjecutar = seleccionarESIPorAlgoritmo(obtenerAlgoritmo());
		pthread_mutex_unlock(&mReady);
		pthread_mutex_unlock(&mEjecutando);

		/*
		 * Este if es porque mientras estaba pausado los ESI en ready
		 * pudieron haberse desconectado o ser bloqueados por consola.
		 */
		if(!esiAEjecutar)
			continue;

		pthread_cancel(esiAEjecutar -> hiloDeFin);
		if(esiAEjecutar -> bufferAux)
		{
			free(esiAEjecutar -> bufferAux);
			esiAEjecutar -> bufferAux = NULL;
		}

		log_info(logger, "El ESI %i está ejecutando con estimacion %f", esiAEjecutar -> id, esiAEjecutar -> estimacion);
		pthread_mutex_lock(&mEnEjecucion);
		ejecucionDeESI(esiAEjecutar);
		pthread_cond_signal(&cFinEjecucion);
		pthread_mutex_unlock(&mEnEjecucion);
	}

	liberarRecursos();
	exit(0);
}

void terminal(socket_t socketStatus)
{
	char *linea, **palabras;
	while(!terminoEjecucion)
	{
		linea = readline("");
		log_info(logger, "[TERMINAL] %s", linea);
		palabras = string_split(linea, " ");
		switch(convertirComando(palabras[0]))
		{
		case pausar:
			sem_wait(&sPausa);
			break;
		case continuar:
			sem_post(&sPausa);
			break;
		case bloquear:
			comandoBloquear(palabras[1], palabras[2]);
			break;
		case desbloquear:
			comandoDesbloquear(palabras[1]);
			break;
		case listar:
			comandoListar(palabras[1]);
			break;
		case kill:
			comandoKill(palabras[1]);
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
	log_info(logger, "ESI %i devolvio %s como resultado", esi -> id, stringResultado);
	free(stringResultado);

	//Controlo que el ESI haya terminado correctamente la instrucción.
	if(*resultado == fallo)
	{
		log_error(logger, "ESI %i fallo durante su ejecución", esi -> id);
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
			log_info(logger, "ESI %i libero %s", esi -> id, ultimaConsulta -> clave);
			ESI* esiLiberado = liberarClave(ultimaConsulta -> clave);
			if(esiLiberado)
			{
				recalcularEstimacion(esiLiberado, obtenerAlfa());
				listarParaEjecucion(esiLiberado);
				sem_post(&contESIEnReady);
				log_info(logger, "ESI %i se agrego a ready con estimacion %f", esiLiberado -> id, esiLiberado -> estimacion);
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
				log_info(logger, "ESI %i se bloqueo por %s", esi -> id, ultimaConsulta -> clave);
			}
			else
			{
				reservarClave(esi, ultimaConsulta -> clave);
				log_info(logger, "ESI %i tomo posesión de %s", esi -> id, ultimaConsulta -> clave);
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
		log_info(logger, "ESI %i termino su ejecución", esi -> id);
		finalizarESIBien(esi);
		return;
	}

	free(resultado);

	actualizarEstimacion(esi);
	if(!bloqueo)
		sem_post(&contESIEnReady);
	crearFinEjecucion(esi);
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
			log_info(logger, "ESI %i se conecto", nuevaESI -> id);
			crearFinEjecucion(nuevaESI);
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

void escucharPorFinESI(ESI* esi)
{
	if(recibirMensaje(esi -> socket, 1, &(esi ->bufferAux)))
	{
		free(esi -> bufferAux);
		log_error("ESI %i se desconecto", esi -> id);
		if(ESIEnReady(esi -> id))
			sem_wait(&contESIEnReady);
		finalizarESIMal(esi);
		pthread_exit(NULL);
	}
}

void comunicacionCoord(socket_t socketCoord)
{
	header *head;
	while(1)
	{
		if(recibirMensaje(socketCoord, sizeof(header), (void**) &head) == 1)
			salirConError("Se desconecto el coordinador");

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

void comandoBloquear(char* clave, char* IdESI)
{
	ESI_id IDparaBloquear = atoi(IdESI);
	ESI* ESIParaBloquear = NULL;

	//Compruebo que parametro ID haya sido un posible ID
	if(IDparaBloquear <= 0)
	{
		log_error(logger, "[TERMINAL] Parametro ID invalido");
		return;
	}

	//Compruebo que sea el ESI ejecutando, espero a que termine la ejecución
	//y despues compruebo que lo siga siendo
	pthread_mutex_lock(&mEjecutando);
	if(esESIEnEjecucion(IDparaBloquear))
	{
		printf("[TERMINAL] El ESI a bloquear está ejecutando, esperando fin de ejecución.\n");
		pthread_mutex_lock(&mEnEjecucion);
		pthread_cond_wait(&cFinEjecucion, &mEnEjecucion);
		pthread_mutex_unlock(&mEnEjecucion);

		if(esESIEnEjecucion(IDparaBloquear))
		{
			ESIParaBloquear = ESIEjecutando();
		}
	}
	pthread_mutex_lock(&mBloqueados);
	pthread_mutex_lock(&mReady);
	if(ESIParaBloquear || (ESIParaBloquear = ESIEnReady(IDparaBloquear)))
	{
		if(claveTomada(clave))
		{
			bloquearESI(ESIParaBloquear, clave);
			log_info(logger, "[TERMINAL] ESI %i se bloqueo por %s", ESIParaBloquear -> id, clave);
		}
		else
		{
			reservarClave(ESIParaBloquear, clave);
			log_info(logger, "[TERMINAL] ESI %i tomo posesion de %s", ESIParaBloquear -> id, clave);
		}
	}
	else if(estaEnFinalizados(IDparaBloquear))
	{
		log_error(logger, "[TERMINAL] ESI %i está finalizado", IDparaBloquear);
	}
	else if((ESIParaBloquear = ESIEstaBloqueado(IDparaBloquear)))
	{
		if(claveTomada(clave))
		{
			log_error(logger, "[TERMINAL] ESI %i bloqueado; %s tomada", IDparaBloquear, clave);
		}
		else
		{
			reservarClave(ESIParaBloquear, clave);
			log_info(logger, "[TERMINAL] ESI %i tomo posesion de %s", ESIParaBloquear -> id, clave);
		}
	}
	else
	{
		log_error(logger, "[TERMINAL] ESI %i no existe", IDparaBloquear);
	}
	pthread_mutex_unlock(&mReady);
	pthread_mutex_unlock(&mBloqueados);
	pthread_mutex_unlock(&mEjecutando);
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
			log_info(logger, "[TERMINAL] %s liberada", clave);
		}
		else
		{
			log_error(logger, "[TERMINAL] %s no está tomada ni tiene ESI bloqueados", clave);
		}
		pthread_mutex_unlock(&mBloqueados);
		return;
	}
	pthread_mutex_unlock(&mBloqueados);

	pthread_mutex_lock(&mReady);
	recalcularEstimacion(ESIDesbloqueado, obtenerAlfa());
	listarParaEjecucion(ESIDesbloqueado);
	log_info(logger, "[TERMINAL] ESI %i se agrego a ready con estimacion %f", ESIDesbloqueado -> id, ESIDesbloqueado -> estimacion);
	pthread_mutex_unlock(&mReady);
	sem_post(&contESIEnReady);
}

void comandoListar(char* clave)
{
	char *lista = string_new();
	void imprimirID(void* id)
	{
		string_append_with_format(&lista, "-%i", *((ESI_id*) id));
	}
	t_list* listaDeID;

	pthread_mutex_lock(&mBloqueados);
	listaDeID = listarID(clave);
	pthread_mutex_unlock(&mBloqueados);

	if(!listaDeID)
	{
		printf(logger, "[TERMINAL] %s no tiene ESI bloqueados", clave);
	}
	else
	{
		list_iterate(listaDeID, imprimirID);
		list_destroy_and_destroy_elements(listaDeID, free);
		printf(logger, "[TERMINAL] %s tiene bloqueados: %s", clave, lista);
	}
	free(lista);
}

void comandoKill(char *IdESI)
{
	ESI_id IDparaMatar = atoi(IdESI);
	ESI* ESIParaMatar = NULL;

	//Compruebo que parametro ID haya sido un posible ID
	if(IDparaMatar <= 0)
	{
		log_error(logger, "[TERMINAL] Parametro ID invalido");
		return;
	}

	//Compruebo que sea el ESI ejecutando, espero a que termine la ejecución
	//y despues compruebo que lo siga siendo
	pthread_mutex_lock(&mEjecutando);
	if(esESIEnEjecucion(IDparaMatar))
	{
		printf("[TERMINAL] El ESI a matar está ejecutando, esperando fin de ejecución.\n");
		pthread_mutex_lock(&mEnEjecucion);
		pthread_cond_wait(&cFinEjecucion, &mEnEjecucion);
		pthread_mutex_unlock(&mEnEjecucion);
		if(esESIEnEjecucion(IDparaMatar))
			ESIParaMatar = ESIEjecutando();
	}
	if(ESIParaMatar || (ESIParaMatar = ESIEnReady(IDparaMatar)) || (ESIParaMatar = ESIEstaBloqueado(IDparaMatar)))
	{
		finalizarESIBien(ESIParaMatar);
		log_info(logger, "[TERMINAL] ESI %s fue finalizado", IdESI);
	}
	else if(estaEnFinalizados(IDparaMatar))
	{
		log_error(logger, "[TERMINAL] ESI %s está finalizado", IdESI);
	}
	else
	{
		log_error(logger, "[TERMINAL] ESI %s no existe", IdESI);
	}
	pthread_mutex_unlock(&mEjecutando);
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
		string_append_with_format(&lista, "-%i", *((ESI_id*) id));
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
	if(crearConfiguracion(dirConfig))
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n");
	inicializarColas();

	logger = log_create("log de planificación", "planificador", true, LOG_LEVEL_INFO);
	//El tercer parametro es el valor inicial del semaforo
	sem_init(&sPausa, 0, 1);
	sem_init(&contESIEnReady, 0, 0);

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
	pthread_mutex_lock(&mBloqueados);
	list_iterate(esi -> recursos, liberarClave);
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
	error_show(error);
	terminoEjecucion = 1;
}
