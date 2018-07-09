#include "planificador.h"
#include "configuracion.h"
#include "colas.h"

sem_t corriendo, contESIEnReady, sSocketCoord;

pthread_mutex_t mReady = PTHREAD_MUTEX_INITIALIZER,
				mBloqueados = PTHREAD_MUTEX_INITIALIZER,
				mFinalizados = PTHREAD_MUTEX_INITIALIZER,
				mEjecutando = PTHREAD_MUTEX_INITIALIZER,
				mEnEjecucion = PTHREAD_MUTEX_INITIALIZER,
				mConsStatus = PTHREAD_MUTEX_INITIALIZER,
				mCondicionStatus = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cFinEjecucion = PTHREAD_COND_INITIALIZER,
				cRecibioStatus = PTHREAD_COND_INITIALIZER;

consultaCoord *ultimaConsulta = NULL;

consultaStatus *consStatus = NULL;

int terminoEjecucion = 0;

int main(int argc, char** argv) {
	inicializacion(argv[1]);

	bloquearClavesConfiguracion();

	socket_t socketNuevasESI = crearServerESI();
	listen(socketNuevasESI, 5);
	crearHiloCoordinador(socketNuevasESI);

	crearHiloTerminal();

	crearHiloNuevasESI(socketNuevasESI);

	ESI * esiAEjecutar;
	while(!terminoEjecucion)
	{
		sem_wait(&contESIEnReady);
		sem_wait(&corriendo);

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

		printf("Se selecciono al ESI %i para ejecutar.\n", esiAEjecutar -> id);
		pthread_mutex_lock(&mEnEjecucion);
		ejecucionDeESI(esiAEjecutar);
		pthread_cond_signal(&cFinEjecucion);
		pthread_mutex_unlock(&mEnEjecucion);

		sem_post(&corriendo);
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
			sem_wait(&corriendo);
			break;
		case continuar:
			sem_post(&corriendo);
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
			comandoStatus(palabras[1]);
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
	header *head;
	enum resultadoEjecucion *resultado;

	enviarEncabezado(esi -> socket, 7); //Enviar el aviso de ejecucion
	printf("Se envio la orden de ejecución.\n");

	sem_post(&sSocketCoord);

	recibirMensaje(esi -> socket, sizeof(header), (void**) &head);
	if(head -> protocolo != 12)
	{ /* ERROR */ }
	free(head);

	recibirMensaje(esi->socket, sizeof(enum resultadoEjecucion), (void**) &resultado);
	printf("Se recibio el resultado de la ejecución.\n");


	//Controlo que el ESI haya terminado correctamente la instrucción.
	if(*resultado == fallo)
	{
		//Esto se repite despues, podria modulizarlo.
		printf("El ESI %i tuvo un fallo en su ejecución y fue terminado.\n", esi -> id);
		free(ultimaConsulta -> clave);
		free(ultimaConsulta);
		ultimaConsulta = NULL;
		finalizarESIMal(esi);
		return;
	}

	//Actualizo el estado del sistema en base a la instrucción.
	switch(ultimaConsulta -> tipo)
	{
		case liberadora:
			if(ultimaConsulta -> resultado)
			{
				pthread_mutex_lock(&mBloqueados);
				pthread_mutex_lock(&mReady);
				ESI* esiLiberado = liberarClave(ultimaConsulta -> clave);
				if(esiLiberado)
				{
					listarParaEjecucion(esiLiberado);
					sem_post(&contESIEnReady);
				}
				pthread_mutex_unlock(&mBloqueados);
				pthread_mutex_unlock(&mReady);
				printf("Se libero la clave %s del ESI %i.\n", ultimaConsulta -> clave, esi -> id);
			}
			break;
		case bloqueante:
			//En realidad acá están mal usados los semaforos porque no están pegados a la region critica.
			pthread_mutex_lock(&mBloqueados);
			if(ultimaConsulta -> resultado)
			{
				colocarEnColaESI(esi, ultimaConsulta -> clave);
				printf("El ESI %i se bloqueo por la clave %s\n", esi -> id, ultimaConsulta -> clave);
			}
			else
			{
				reservarClave(esi, string_duplicate(ultimaConsulta -> clave));
				printf("El ESI %i tomo posesión de la clave %s\n", esi -> id, ultimaConsulta -> clave);
			}
			pthread_mutex_unlock(&mBloqueados);
			break;
		case noDefinido:
			break;
	}

	//Compruebo si el ESI termino su ejecución.
	if(*resultado == fin)
	{
		free(ultimaConsulta -> clave);
		free(ultimaConsulta);
		ultimaConsulta = NULL;
		printf("El ESI %i finalizo su ejecución.", esi -> id);
		finalizarESIBien(esi);
		return;
	}

	free(ultimaConsulta -> clave);
	free(ultimaConsulta);
	free(resultado);
	ultimaConsulta = NULL;

	//El ESI sigue siendo valido para el planificador por lo tanto se puede llegar a desconectar.
	sem_post(&contESIEnReady);
	crearFinEjecucion(esi);
}

void escucharPorESI (socket_t socketServerESI)
{
	socket_t socketNuevaESI;
	ESI* nuevaESI;

	while(!terminoEjecucion)
	{
		socketNuevaESI = ERROR;
		listen(socketServerESI, 5);
		socketNuevaESI = aceptarConexion(socketServerESI);
		if(socketNuevaESI == ERROR)
		{
			error_show("Fallo en la conexion de nuevo ESI\n");
			continue;
		}
		nuevaESI = crearESI(socketNuevaESI, obtenerEstimacionInicial());
		if(!enviarIdESI(socketNuevaESI, nuevaESI -> id))
		{
			crearFinEjecucion(nuevaESI);
			pthread_mutex_lock(&mReady);
			listarParaEjecucion(nuevaESI);
			pthread_mutex_unlock(&mReady);
			sem_post(&contESIEnReady);
			printf("Se conecto un nuevo ESI.\n");
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
			printf("El ESI %i se desconecto.", esi -> id);
			if(ESIEnReady(esi -> id))
				sem_wait(&contESIEnReady);
			finalizarESIMal(esi);
			pthread_exit(NULL);
		}
	}
}

void comunicacionCoord(socket_pair_t *socketCoord)
{
	header *head;
	while(1)
	{

		sem_wait(&sSocketCoord);

		pthread_mutex_lock(&mConsStatus);
		if(!consStatus)
		{
			pthread_mutex_unlock(&mConsStatus);
			if(recibirMensaje(socketCoord -> escucha, sizeof(header), (void**) &head) == 1)
				salirConError("Se desconecto el coordinador.\n");
			//Consulta estado clave
			printf("Se recibio una consulta del coordinador.\n");
			ultimaConsulta = recibirConsultaCoord(socketCoord -> escucha);
			if(ultimaConsulta -> tipo == bloqueante)
				ultimaConsulta -> resultado = claveTomada(ultimaConsulta -> clave);
			else
				ultimaConsulta -> resultado = claveTomadaPorESI(ultimaConsulta -> clave, ultimaConsulta -> id_esi);
			enviarRespuestaConsultaCoord(socketCoord -> escucha, ultimaConsulta -> resultado);
		}
		else
		{
			pthread_mutex_unlock(&mConsStatus);
			head -> protocolo = 13;
			enviarHeader(socketCoord -> envio, *head);
			enviarBuffer(socketCoord -> envio, (void*) &(consStatus -> tamClave), sizeof(tamClave_t));
			enviarBuffer(socketCoord -> envio, (void*) consStatus -> clave, consStatus -> tamClave);

			recibirRespuestaStatus(socketCoord -> envio, consStatus);

			pthread_mutex_lock(&mCondicionStatus);
			pthread_cond_signal(&cRecibioStatus);
			pthread_mutex_unlock(&mCondicionStatus);
		}
		free(head);
	}
}

void comandoBloquear(char* clave, char* IdESI)
{
	ESI_id IDparaBloquear = atoi(IdESI);
	ESI* ESIParaBloquear = NULL;

	//Compruebo que parametro ID haya sido un posible ID
	if(IDparaBloquear > 0)
	{
		printf("El parametro ID no es valido.\n");
		return;
	}

	//Compruebo que sea el ESI ejecutando, espero a que termine la ejecución
	//y despues compruebo que lo siga siendo
	pthread_mutex_lock(&mEjecutando);
	if(esESIEnEjecucion(IDparaBloquear))
	{
		printf("El ESI a bloquear está ejecutando, esperando fin de ejecución.\n");
		pthread_mutex_lock(&mEnEjecucion);
		pthread_cond_wait(&cFinEjecucion, &mEnEjecucion);
		if(esESIEnEjecucion(IDparaBloquear))
			ESIParaBloquear = ESIEjecutando();
	}
	pthread_mutex_lock(&mBloqueados);
	pthread_mutex_lock(&mReady);
	if(ESIParaBloquear || (ESIParaBloquear = ESIEnReady(IDparaBloquear)))
	{
		if(claveTomada(clave))
		{
			bloquearESI(ESIParaBloquear, clave);
			printf("La clave %s ya se encuentra tomada, el ESI %i fue bloqueada.\n", clave, ESIParaBloquear -> id);
		}
		else
		{
			reservarClave(ESIParaBloquear, clave);
			printf("La clave %s fue tomada por el ESI %i.\n", clave, ESIParaBloquear -> id);
		}
	}
	else if(estaEnFinalizados(IDparaBloquear))
	{
		printf("El ESI %i ya está finalizado.\n", IDparaBloquear);
	}
	else if((ESIParaBloquear = ESIEstaBloqueado(IDparaBloquear)))
	{
		if(claveTomada(clave))
		{
			printf("La clave %s ya se encuentra tomada, el ESI %i ya se encontraba bloqueado.\n", clave, ESIParaBloquear -> id);
		}
		else
		{
			reservarClave(ESIParaBloquear, clave);
			printf("La clave %s fue tomada por el ESI %i.\n", clave, ESIParaBloquear -> id);
		}
	}
	else
	{
		printf("El ESI %i no existe en el sistema.\n", IDparaBloquear);
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
		printf("No hay ESI bloqueados para la clave %s.\n", clave);
		if(claveTomada(clave))
		{
			liberarClave(clave); //No necesito leer el valor de retorno porque sé que es NULL
			printf("Se libero la clave %s.\n", clave);
		}
		else
		{
			printf("La clave %s no se encuentra tomada.\n", clave);
		}
		pthread_mutex_unlock(&mBloqueados);
		return;
	}
	pthread_mutex_unlock(&mBloqueados);

	pthread_mutex_lock(&mReady);
	listarParaEjecucion(ESIDesbloqueado);
	pthread_mutex_unlock(&mReady);
	sem_post(&contESIEnReady);
	printf("Se agrego a Ready el ESI %i", ESIDesbloqueado -> id);
}

void comandoListar(char* clave)
{
	void imprimirID(void* id)
	{
		printf("-%i\n", *((ESI_id*) id));
	}
	t_list* listaDeID;

	pthread_mutex_lock(&mBloqueados);
	listaDeID = listarID(clave);
	pthread_mutex_unlock(&mBloqueados);

	if(!listaDeID)
	{
		printf("No hay ESI bloqueados con esa clave.\n");
	}
	else
	{
		printf("Los ESI bloqueados para la clave \"%s\" son:\n", clave);
		list_iterate(listaDeID, imprimirID);
		list_destroy_and_destroy_elements(listaDeID, free);
	}
}

void comandoKill(char *IdESI)
{
	ESI_id IDparaMatar = atoi(IdESI);
	ESI* ESIParaMatar = NULL;

	//Compruebo que parametro ID haya sido un posible ID
	if(IDparaMatar > 0)
	{
		printf("El parametro ID no es valido.\n");
		return;
	}

	//Compruebo que sea el ESI ejecutando, espero a que termine la ejecución
	//y despues compruebo que lo siga siendo
	pthread_mutex_lock(&mEjecutando);
	if(esESIEnEjecucion(IDparaMatar))
	{
		printf("El ESI a matar está ejecutando, esperando fin de ejecución.\n");
		pthread_mutex_lock(&mEnEjecucion);
		pthread_cond_wait(&cFinEjecucion, &mEnEjecucion);
		if(esESIEnEjecucion(IDparaMatar))
			ESIParaMatar = ESIEjecutando();
	}
	if(ESIParaMatar || (ESIParaMatar = ESIEnReady(IDparaMatar)) || (ESIParaMatar = ESIEstaBloqueado(IDparaMatar)))
	{
		finalizarESIBien(ESIParaMatar);
		printf("El ESI %s fue finalizado.\n", IdESI);
	}
	else if(estaEnFinalizados(IDparaMatar))
	{
		printf("El ESI %s ya está finalizado.\n", IdESI);
	}
	else
	{
		printf("El ESI %s no existe en el sistema.\n", IdESI);
	}
	pthread_mutex_unlock(&mEjecutando);
}

void comandoStatus(char* clave)
{
	printf("Preguntando al coordinador por el estado de la clave %s", clave);

	pthread_mutex_lock(&mConsStatus);
	consStatus = malloc(sizeof(consultaStatus));
	consStatus -> clave = clave;
	consStatus -> tamClave = string_length(clave) + 1;
	pthread_mutex_unlock(&mConsStatus);

	sem_post(&sSocketCoord);

	pthread_mutex_lock(&mCondicionStatus);
	pthread_cond_wait(&cRecibioStatus, &mCondicionStatus);
	pthread_mutex_unlock(&mCondicionStatus);
	ESI *dueno;
	if(claveTomadaPor(clave, &dueno))
	{
		printf("La clave %s se encuentra tomada por el ", clave);
		if(dueno)
			printf("ESI %i", dueno -> id);
		else
			printf("sistema");
		printf(".\n");
	}
	else
		printf("La clave %s no se encuentra tomada por nadie.\n", clave);
	switch(consStatus -> estado)
	{
	case existente:
		printf("La clave %s se encuentra en la instancia %i (%s) y su valor actual %s.\n", clave,
				consStatus -> id, consStatus -> nombre, consStatus -> valor);
		free(consStatus -> valor);
		break;
	case innexistente:
		printf("La clave %s no existe en el coordinador, actualmente iria a la instancia %i (%s).\n", clave,
				consStatus -> id, consStatus -> nombre);
		break;
	case sinIniciar:
		printf("La clave %s no tiene instacia asignada, actualmente iria a la instancia %i (%s).\n", clave,
				consStatus -> id, consStatus -> nombre);
		break;
	case caida:
		printf("La clave %s se encuentra en la instancia %i (%s) que está desconectada.\n", clave,
				consStatus -> id, consStatus -> nombre);
		break;
	case sinValor:
		printf("La clave %s se encontraba en la instancia %i (%s) pero fue reemplazada.\n", clave,
				consStatus -> id, consStatus -> nombre);
		break;
	}
	comandoListar(clave);
	free(consStatus -> nombre);
	free(consStatus);
	consStatus = NULL;
}

void comandoDeadlock()
{
	void imprimirID(void* id)
	{
		printf("-%i\n", *((ESI_id*) id));
	}
	t_list* esiEnDeadlock = NULL;
	printf("Analizando existencia de deadlock...\n");
	pthread_mutex_lock(&mBloqueados);
	esiEnDeadlock = detectarDeadlock();
	pthread_mutex_unlock(&mBloqueados);
	if(!esiEnDeadlock)
		printf("No hay deadlock");
	else
	{
		printf("Existe deadlock. ESI involucrados:\n");
		list_iterate(esiEnDeadlock, imprimirID);
	}
	list_destroy_and_destroy_elements(esiEnDeadlock, free);
}

void inicializacion (char* dirConfig)
{
	if(crearConfiguracion(dirConfig))
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n");
	inicializarColas();

	//El tercer parametro es el valor inicial del semaforo
	sem_init(&corriendo, 0, 1);
	sem_init(&contESIEnReady, 0, 0);

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

booleano enviarIdESI(socket_t sock, ESI_id id)
{
	booleano resultado = 1;
	resultado &= enviarEncabezado(sock, 6);
	resultado &= enviarBuffer(sock, (void*) &id, sizeof(id));
	return resultado;
}

int enviarRespuestaConsultaCoord(socket_t coord, booleano respuesta)
{
	return enviarEncabezado(coord, 10) & enviarBuffer(coord, &respuesta, sizeof(booleano));
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
	recibirMensaje(socketCoord, sizeof(header) + sizeof(instancia_id) + sizeof(tamNombreInstancia_t), &buffer);
	if(((header*) buffer) -> protocolo != 14)
	{ /*ERROR*/}
	free(buffer);

	recibirMensaje(socketCoord, sizeof(instancia_id) + sizeof(tamNombreInstancia_t), &buffer);
	status -> id = *((instancia_id*) buffer);
	status -> tamNombre = *((tamNombreInstancia_t*) buffer + sizeof(instancia_id));
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

pthread_t crearHiloCoordinador(socket_t socketEscucha)
{
	socket_t socketCoord = ERROR, socketStatus = ERROR;
	pthread_t hilo;
	pthread_attr_t attr;

	socketCoord = conectarConCoordinador();

	tamClave_t tamIP = string_length(obtenerIP()) + sizeof(char), tamPuerto = string_length(obtenerPuerto()) + sizeof(char);

	if(socketCoord == ERROR)
		salirConError("No se pudo conectar con el coordinador.\n");
	if(enviarEncabezado(socketCoord, 1) || enviarBuffer(socketCoord, &tamIP, sizeof(tamClave_t)) || enviarBuffer(socketCoord, obtenerIP(), tamIP) ||
		enviarBuffer(socketCoord, &tamPuerto, sizeof(tamClave_t)) || enviarBuffer(socketCoord, obtenerPuerto(), tamPuerto)) //Enviar el handshake
		salirConError("Fallo al enviar el handshake planificador -> Coordinador\n");

	socketStatus = aceptarConexion(socketEscucha);
	printf("Se conecto el socket para status.\n");
	socket_pair_t *par = malloc(sizeof(socket_pair_t));
	par -> escucha = socketCoord;
	par -> envio = socketStatus;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&hilo, &attr, (void*) comunicacionCoord, (void*) par);
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
	liberarRecursosDeESI(esi);
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
	sem_destroy(&corriendo);
	sem_destroy(&contESIEnReady);

}

void salirConError(char * error)
{
	error_show(error);
	terminoEjecucion = 1;
}
