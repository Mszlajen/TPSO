#include "coordinador.h"

t_dictionary *claves = NULL; // (clave, instancia_t)
t_list *instancias = NULL; // instancia_t
t_config *configuracion = NULL;
t_log *logger = NULL;

sem_t sSocketPlanificador, sTerminoConsulta, sTerminoEjecucion;
pthread_mutex_t mClaves = PTHREAD_MUTEX_INITIALIZER,
				mInstancias = PTHREAD_MUTEX_INITIALIZER;

operacion_t operacion;
consultaStatus_t consultaStatus;

int punteroSeleccion = 0;
instancia_id contadorInstancias = 0;
booleano compactar = 0;

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		error_show("Cantidad de argumentos invalida.\n");
		return ERROR;
	}

	inicializacion(argv[1]);

	char* IP = config_get_string_value(configuracion, IPEscucha);
	char* puerto = config_get_string_value(configuracion, Puerto);
	socket_t socketCoordinador = crearSocketServer(IP, puerto);
	if (socketCoordinador == ERROR) {
		error_show("Fallo al crear el socket escucha.");
		exit(ERROR);
	}
	listen(socketCoordinador, 5);

	booleano esPlanificador;
	do {
		esPlanificador = conectarConPlanificador(socketCoordinador);
	} while (esPlanificador);

	recibirNuevasConexiones(socketCoordinador);

}

void inicializacion(char *dirConfiguracion) {
	configuracion = config_create(dirConfiguracion);
	if (!configuracion) {
		error_show("No se pudo abrir la configuracion.\n");
		exit(ERROR);
	}
	claves = dictionary_create();
	instancias = list_create();

	logger = log_create("log de operaciones", "coordinador", 1, LOG_LEVEL_INFO);
	sem_init(&sSocketPlanificador, 0, 0);
	sem_init(&sTerminoConsulta, 0, 0);
	sem_init(&sTerminoEjecucion, 0, 0);

	consultaStatus.atender = 0;
	sem_init(&consultaStatus.atendida, 0, 0);
	consultaStatus.clave = NULL;
	consultaStatus.valor = NULL;

	limpiarOperacion();
}

socket_t conectarConPlanificador(socket_t socketEscucha) {
	socket_t *socketAceptada = malloc(sizeof(socket_t));
	*socketAceptada = aceptarConexion(socketEscucha);
	//printf("Se realizo una conexion, comprobando si es el planificador.\n");

	header* handshake = NULL;
	int estadoDellegada;
	pthread_t pHiloPlanificador, pHiloStatus;

	estadoDellegada = recibirMensaje(*socketAceptada, sizeof(header),
			(void**) &handshake);
	if (estadoDellegada) {
		//error_show("No se pudo recibir header de la conexion.\n");
		close(*socketAceptada);
		free(socketAceptada);
		return ERROR;
	}
	if (handshake->protocolo != 1) {
		//error_show("La conexion no es el planificador, descartando.\n");
		close(*socketAceptada);
		free(socketAceptada);
		return ERROR;
	}
	//printf("La conexion es el planificador, creando conexion para consulta de status.\n");
	tamClave_t *tamIP, *tamPuerto;
	char *ip;
	char *puerto;
	recibirMensaje(*socketAceptada, sizeof(tamClave_t), (void**) &tamIP);
	recibirMensaje(*socketAceptada, *tamIP, (void**) &ip);
	recibirMensaje(*socketAceptada, sizeof(tamClave_t), (void**) &tamPuerto);
	recibirMensaje(*socketAceptada, *tamPuerto, (void**) &puerto);
	free(tamIP);
	free(tamPuerto);
	socket_t *socketStatus = malloc(sizeof(socket_t));
	*socketStatus = crearSocketCliente(ip, puerto);
	if (*socketStatus == ERROR) {
		error_show("No se pudo crear la conexion para consulta de status.\n");
		close(*socketAceptada);
		free(socketAceptada);
		return ERROR;
	}
	pthread_create(&pHiloPlanificador, NULL, (void*) hiloPlanificador,
			(void*) socketAceptada);
	pthread_create(&pHiloStatus, NULL, (void*) hiloStatus,
			(void*) socketStatus);
	free(ip);
	free(puerto);
	//printf("Se creo la conexion para consulta de status.\n");
	return EXITO;
}

void recibirNuevasConexiones(socket_t socketEscucha) {
	header *head;
	socket_t socketAceptado;
	int error;
	while (1) {
		socketAceptado = aceptarConexion(socketEscucha);
		if (socketAceptado == ERROR)
			continue;
		error = recibirMensaje(socketAceptado, sizeof(header), (void**) &head);
		if (error) {
			free(head);
			close(socketAceptado);
			continue;
		}

		switch (head->protocolo) {
		case 2:
			//printf("Se recibio el pedido de conexion de una instancia.\n");
			registrarInstancia(socketAceptado);
			break;
		case 3:
			//printf("Se recibio el pedido de conexion de una ESI.\n");
			registrarESI(socketAceptado);
			break;
		default:
			close(socketAceptado);
		}

		free(head);
	}
}

void registrarInstancia(socket_t conexion)
{
	instancia_id *id;
	recibirMensaje(conexion, sizeof(instancia_id), (void**) &id);

	tamNombreInstancia_t *tamNombre;
	recibirMensaje(conexion, sizeof(tamNombreInstancia_t), (void**) &tamNombre);

	char *nombre;
	recibirMensaje(conexion, *tamNombre, (void**) &nombre);
	free(tamNombre);

	instancia_t *nuevaInstancia = NULL;
	if(*id)
	{
		bool esInstancia(void* instancia) {
			return ((instancia_t *) instancia)->id == *id;
		}

		nuevaInstancia = list_find(instancias, esInstancia);
	}

	enviarEncabezado(conexion, nuevaInstancia? 4 : 5);

	if (nuevaInstancia)
	{
		if(nuevaInstancia -> conectada)
		{
			pthread_cancel(nuevaInstancia -> hilo);
			sem_post(&nuevaInstancia -> sIniciarEjecucion);
			close(nuevaInstancia -> socket);
		}

		void enviarClave(void* clave)
		{
			tamClave_t tam = string_length((char*) clave) + sizeof(char);
			enviarBuffer(conexion, &tam, sizeof(tamClave_t));
			enviarBuffer(conexion, clave, sizeof(tamClave_t));
		}
		free(nuevaInstancia->nombre);
		nuevaInstancia->nombre = nombre;

		cantClaves_t cantidad = list_size(nuevaInstancia->claves);

		enviarBuffer(conexion, (void*) &cantidad, sizeof(cantClaves_t));
		list_iterate(nuevaInstancia->claves, enviarClave);

		cantClaves_t *entradasLibres;
		recibirMensaje(conexion, sizeof(cantClaves_t), (void**) &entradasLibres);
		nuevaInstancia->entradasLibres = *entradasLibres;
		free(entradasLibres);
	}
	else
	{
		nuevaInstancia = malloc(sizeof(instancia_t));
		nuevaInstancia->claves = list_create();
		sem_init(&(nuevaInstancia->sIniciarEjecucion), 0, 0);
		sem_init(&(nuevaInstancia->sFinCompactacion), 0, 0);
		contadorInstancias++;
		nuevaInstancia->id = contadorInstancias;
		nuevaInstancia->nombre = nombre;
		nuevaInstancia->entradasLibres = config_get_int_value(configuracion, "CantEntradas");
	}
	nuevaInstancia->conectada = 1;
	nuevaInstancia->socket = conexion;

	enviarBuffer(conexion, (void*) &nuevaInstancia -> id, sizeof(instancia_id));
	enviarInfoEntradas(conexion);

	pthread_mutex_lock(&mInstancias);
	list_add(instancias, nuevaInstancia);

	pthread_t hilo;
	pthread_create(&hilo, NULL, (void*) hiloInstancia, (void*) nuevaInstancia);
	pthread_mutex_unlock(&mInstancias);
	nuevaInstancia -> hilo = hilo;
	free(id);
}

void registrarESI(socket_t conexion)
{
	socket_t *socketESI = malloc(sizeof(socket_t));
	*socketESI = conexion;
	pthread_t hilo;
	pthread_create(&hilo, NULL, (void*) hiloESI, (void*) socketESI);
}

void hiloInstancia(instancia_t *instancia)
{
	header *head;
	enum resultadoEjecucion *result;
	while (1) {
		sem_wait(&instancia -> sIniciarEjecucion);

		if(compactar)
		{
			if (seDesconectoSocket(instancia->socket)) {
				log_info(logger, "Se desconecto la instancia %s, ID = %i",
							instancia->nombre, instancia->id);
				instancia->conectada = 0;
				close(instancia->socket);
				sem_post(&(instancia->sFinCompactacion));
				pthread_exit(0);
			}
			enum instruccion instr = compactacion;
			enviarEncabezado(instancia->socket, 11);
			enviarBuffer(instancia->socket, (void*) &instr,
					sizeof(enum instruccion));

			recibirMensaje(instancia->socket, sizeof(header), (void**) &head);
			recibirMensaje(instancia->socket, sizeof(enum resultadoEjecucion),
					(void**) &result);
			sem_post(&(instancia->sFinCompactacion));
			continue;
		}

		if(consultaStatus.atender)
		{
			if(seDesconectoSocket(instancia -> socket))
			{
				consultaStatus.estado = caida;

				sem_post(&consultaStatus.atendida);
				return;
			}
			enviarEncabezado(instancia->socket, 13);
			enviarBuffer(instancia->socket, (void*) &consultaStatus.tamClave, sizeof(tamClave_t));
			enviarBuffer(instancia->socket, (void*) consultaStatus.clave, consultaStatus.tamClave);

			recibirMensaje(instancia->socket, sizeof(header), (void**) &head);
			free(head);
			booleano *existe;
			recibirMensaje(instancia->socket, sizeof(booleano), (void**) &existe);

			if(*existe)
			{
				tamValor_t *tamValor;
				recibirMensaje(instancia->socket, sizeof(tamValor_t), (void**) &tamValor);
				recibirMensaje(instancia->socket, *tamValor, (void**) &consultaStatus.valor);
				consultaStatus.tamValor = *tamValor;
				free(tamValor);
				consultaStatus.estado = existente;
			}
			free(existe);
			consultaStatus.atender = 0;
			sem_post(&consultaStatus.atendida);
			continue;
		}

		if (seDesconectoSocket(instancia->socket))
		{
			log_info(logger, "Se desconecto la instancia %s, ID = %i",
					instancia->nombre, instancia->id);
			instancia->conectada = 0;
			close(instancia->socket);
			sem_post(&sTerminoEjecucion);
			pthread_exit(0);
		}

		enviarEncabezado(instancia->socket, 11);
		enviarBuffer(instancia->socket, (void*) &operacion.instr,
				sizeof(enum instruccion));
		enviarBuffer(instancia->socket, (void*) &operacion.tamClave,
				sizeof(tamClave_t));
		enviarBuffer(instancia->socket, (void*) operacion.clave,
				operacion.tamClave);
		if (operacion.instr == set || operacion.instr == create) {
			enviarBuffer(instancia->socket, (void*) &operacion.tamValor,
					sizeof(tamValor_t));
			enviarBuffer(instancia->socket, (void*) operacion.valor,
					operacion.tamValor);
		}

		recibirMensaje(instancia->socket, sizeof(header), (void**) &head);
		free(head);
		recibirMensaje(instancia->socket, sizeof(enum resultadoEjecucion),
				(void**) &result);

		if (operacion.instr == set || operacion.instr == create)
		{
			cantEntradas_t *entradasDisponibles;
			recibirMensaje(instancia->socket, sizeof(cantEntradas_t), (void**) &entradasDisponibles);
			instancia->entradasLibres = *entradasDisponibles;
			free(entradasDisponibles);
		}

		if (*result == necesitaCompactar)
		{
			free(result);
			pthread_t hilo;
			pthread_create(&hilo, NULL, (void*) hiloCompactacion, (void*) instancia);
			continue;
		}

		operacion.result = *result;
		free(result);
		sem_post(&sTerminoEjecucion);
	}
}

void hiloCompactacion(instancia_t *llamadora) {
	void activarInstancias(void *instancia) {
		sem_post(&((instancia_t*) instancia)->sIniciarEjecucion);
	}
	void esperarFinCompactacion(void *instancia) {
		sem_wait(&((instancia_t*) instancia)->sFinCompactacion);
	}
	compactar = 1;
	pthread_mutex_lock(&mInstancias);
	list_iterate(instancias, activarInstancias);
	list_iterate(instancias, esperarFinCompactacion);
	pthread_mutex_unlock(&mInstancias);
	compactar = 0;
	sem_post(&(llamadora->sIniciarEjecucion));
}

void hiloESI(socket_t *socketESI) {
	ESI_id *id;
	tamClave_t * tamClave = NULL;
	tamValor_t * tamValor = NULL;
	header * head;
	enum instruccion * instr = NULL;

	while (1) {
		if (recibirMensaje(*socketESI, sizeof(header), (void**) &head))
		{
			//printf("Se desconecto una ESI");
			free(head);
			pthread_exit(0);
		}
		//Aca iria una comprobación del header.
		free(head);
		recibirMensaje(*socketESI, sizeof(ESI_id), (void**) &id);
		operacion.id = *id;
		free(id);
		recibirMensaje(*socketESI, sizeof(enum instruccion), (void**) &instr);
		operacion.instr = *instr;
		free(instr);
		recibirMensaje(*socketESI, sizeof(tamClave_t), (void**) &tamClave);
		operacion.tamClave = *tamClave;
		recibirMensaje(*socketESI, *tamClave, (void**) &operacion.clave);
		free(tamClave);
		if (operacion.instr == set) {
			recibirMensaje(*socketESI, sizeof(tamValor_t), (void**) &tamValor);
			operacion.tamValor = *tamValor;
			recibirMensaje(*socketESI, *tamValor, (void**) &operacion.valor);
			free(tamValor);
			log_info(logger, "ESI %i: %s %s %s", operacion.id,
					instrToArray(operacion.instr), operacion.clave,
					operacion.valor);
		}
		else
			log_info(logger, "ESI %i: %s %s", operacion.id,
					instrToArray(operacion.instr), operacion.clave);

		if (operacion.instr != get
				&& !dictionary_has_key(claves, operacion.clave)) {
			enviarResultado(*socketESI, fallo);
			log_info(logger, "ESI %i: Fallo por clave no identificada",
					operacion.id);
			limpiarOperacion();
			pthread_exit(0);
		}

		sem_post(&sSocketPlanificador);

		sem_wait(&sTerminoConsulta);

		//Empieza división por instruccion
		switch(operacion.instr)
		{
		case get:
			ejecutarGET();
			break;
		case set:
			ejecutarSET();
			break;
		case store:
			ejecutarSTORE();
			break;
		}

		//El retardo es en milisegundos y usleep recibe microsegundos así que convertimos la unidad al pasar el parametro
		usleep((unsigned int)config_get_int_value(configuracion, "Retardo") * 1000);
		enviarResultado(*socketESI, operacion.result);
	}
}

void hiloPlanificador(socket_t *socketPlanificador) {
	header *head;
	booleano *estado;
	tamClave_t tamClave;
	enum tipoDeInstruccion tipo;
	while (1) {
		sem_wait(&sSocketPlanificador);

		//printf("Se está consultando al planificador por la clave %s\n", operacion.clave);

		tamClave = string_length(operacion.clave) + sizeof(char);
		switch (operacion.instr) {
		case store:
			tipo = liberadora;
			break;
		case get:
			tipo = bloqueante;
			break;
		case set:
			tipo = noDefinido;
			break;
		case compactacion:
			break;
		case create:
			break;
		}

		enviarEncabezado(*socketPlanificador, 9);
		enviarBuffer(*socketPlanificador, &(operacion.id), sizeof(ESI_id));
		enviarBuffer(*socketPlanificador, &tipo, sizeof(enum tipoDeInstruccion));
		enviarBuffer(*socketPlanificador, &tamClave, sizeof(tamClave_t));
		enviarBuffer(*socketPlanificador, operacion.clave, tamClave);

		recibirMensaje(*socketPlanificador, sizeof(header), (void **) &head);
		recibirMensaje(*socketPlanificador, sizeof(booleano), (void **) &estado);

		operacion.validez = *estado;

		free(estado);
		free(head);

		//printf("Se recibio la respuesta del planificador (Respuesta = %i).\n", operacion.validez);
		sem_post(&sTerminoConsulta);
	}
}

void hiloStatus(socket_t *socketStatus)
{
	printf("Se conecto el socket de status.\n");
	header *head;
	tamClave_t *tamClave;
	instancia_t *instancia;
	tamNombreInstancia_t tamNombre;
	while (1)
	{
		if(!recibirMensaje(*socketStatus, sizeof(header), (void**) &head))
			salirConError("Se desconecto el planificador.\n");
		free(head);
		recibirMensaje(*socketStatus, sizeof(tamClave_t), (void**) &tamClave);
		recibirMensaje(*socketStatus, *tamClave, (void**) &consultaStatus.clave);
		consultaStatus.tamClave = *tamClave;
		free(tamClave);

		pthread_mutex_lock(&mClaves);
		if(!dictionary_has_key(claves, consultaStatus.clave))
		{
			pthread_mutex_lock(&mInstancias);
			instancia = simularDistribucion();
			pthread_mutex_unlock(&mInstancias);
			consultaStatus.estado = innexistente;
		}
		else
		{
			instancia = dictionary_get(claves, consultaStatus.clave);
			if(!instancia)
			{
				pthread_mutex_lock(&mInstancias);
				instancia = simularDistribucion();
				pthread_mutex_unlock(&mInstancias);
				consultaStatus.estado = sinIniciar;
			}
			else if(instancia -> conectada)
			{
				consultaStatus.atender = 1;
				sem_post(&instancia -> sIniciarEjecucion);
				sem_wait(&consultaStatus.atendida);
			}
			else
			{
				consultaStatus.estado = caida;
			}
		}
		pthread_mutex_unlock(&mClaves);

		tamNombre = string_length(instancia -> nombre) + sizeof(char);

		enviarEncabezado(*socketStatus, 14);
		enviarBuffer(*socketStatus, (void*) &instancia -> id, sizeof(instancia_id));
		enviarBuffer(*socketStatus, (void*) &tamNombre, sizeof(tamNombreInstancia_t));
		enviarBuffer(*socketStatus, (void*) instancia -> nombre, tamNombre);
		enviarBuffer(*socketStatus, (void*) &consultaStatus.estado, sizeof(enum estadoClave));
		if(consultaStatus.estado == existente)
		{
			enviarBuffer(*socketStatus, (void*) &consultaStatus.tamValor, sizeof(tamValor_t));
			enviarBuffer(*socketStatus, (void*) consultaStatus.valor, consultaStatus.tamValor);
			free(consultaStatus.valor);
			consultaStatus.valor = NULL;
		}

		free(consultaStatus.clave);
		consultaStatus.clave = NULL;
	}
}

void ejecutarGET()
{
	if(!dictionary_has_key(claves, operacion.clave))
	{
		pthread_mutex_lock(&mClaves);
		dictionary_put(claves, operacion.clave, NULL);
		pthread_mutex_unlock(&mClaves);
		log_info(logger, "Se creo la clave %s en la tabla de claves", operacion.clave);
	}

	if(operacion.validez)
	{
		operacion.result = bloqueo;
		log_info(logger, "ESI %i: no pudo tomar la clave", operacion.id);
	}
	else
	{
		log_info(logger, "ESI %i: tomo posesión de la clave", operacion.id);
		operacion.result = exito;
	}
}

void ejecutarSET()
{
	if (!operacion.validez) {
		operacion.result = fallo;
		log_info(logger, "ESI %i: Fallo por clave no bloqueada",
				operacion.id);
		return;
	}

	instancia_t *instancia = dictionary_get(claves, operacion.clave);
	if(!instancia)
	{
		return ejecutarCreate();
	}

	sem_post(&instancia -> sIniciarEjecucion);

	sem_wait(&sTerminoEjecucion);

	if(!instancia -> conectada)
	{
		pthread_mutex_lock(&mClaves);
		removerClave(instancia, operacion.clave);
		pthread_mutex_unlock(&mClaves);
		log_info(logger, "ESI %i: Fallo por clave inaccesible", operacion.id);
		operacion.result = fallo;
		return;
	}

	log_info(logger, "ESI %i: Modifico el valor de la clave", operacion.id);
}

void ejecutarCreate()
{
	instancia_t *instancia;
	operacion.instr = create;
	do
	{
		pthread_mutex_lock(&mClaves);
		dictionary_remove(claves, operacion.clave);
		pthread_mutex_lock(&mInstancias);
		instancia = correrAlgoritmo(&punteroSeleccion);
		pthread_mutex_unlock(&mInstancias);
		dictionary_put(claves, operacion.clave, instancia);
		pthread_mutex_unlock(&mClaves);

		sem_post(&instancia -> sIniciarEjecucion);

		sem_wait(&sTerminoEjecucion);
	}while(!instancia -> conectada);
	log_info(logger, "ESI %i: Creo la clave", operacion.id);
}

void ejecutarSTORE()
{
	if (!operacion.validez) {
		operacion.result = fallo;
		log_info(logger, "ESI %i: Fallo por clave no bloqueada",
				operacion.id);
		return;
	}
	instancia_t *instancia = dictionary_get(claves, operacion.clave);

	sem_post(&instancia -> sIniciarEjecucion);

	sem_wait(&sTerminoEjecucion);

	if(!instancia -> conectada)
	{
		pthread_mutex_lock(&mClaves);
		removerClave(instancia, operacion.clave);
		pthread_mutex_unlock(&mClaves);
		log_info(logger, "ESI %i: Fallo por clave inaccesible", operacion.id);
		operacion.result = fallo;
		return;
	}

	if(operacion.result == fallo)
	{
		log_info(logger, "ESI %i: No se pudo bajar la clave a disco", operacion.id);
		return;
	}

	log_info(logger, "ESI %i: Bajo la clave a disco", operacion.id);
}

//Asumo que existe al menos una instancia a donde mandarlo.
instancia_t* correrAlgoritmo(int *puntero)
{
	char * algoritmo = config_get_string_value(configuracion, "Algoritmo");
	instancia_t *instanciaSeleccionada;
	if(string_equals_ignore_case(algoritmo, "EL"))
	{
		instanciaSeleccionada = algoritmoEquitativeLoad(puntero);
	}
	else if(string_equals_ignore_case(algoritmo, "LSU"))
	{
		instanciaSeleccionada = algoritmoLeastSpaceUsed(puntero);
	}
	else
	{
		//Aca iria el key explicit
	}

	return instanciaSeleccionada;
}

instancia_t* algoritmoEquitativeLoad(int *puntero)
{
	instancia_t *actual = NULL, *retorno = NULL;

	do
	{
		if(*puntero >= list_size(instancias))
			*puntero = 0;
		actual = list_get(instancias, *puntero);
		if(actual -> conectada)
		{
			retorno = actual;
		}
		*puntero += 1;
	}while(!retorno);

	return retorno;
}

instancia_t* algoritmoLeastSpaceUsed(int *puntero)
{
	instancia_t *retorno = list_get(instancias, 0);
	cantEntradas_t espacioLibre = retorno -> entradasLibres;
	int posicionActual = 0, posicionRetorno = 0;
	booleano huboDesempate = 0;
	void buscarCandidatos(instancia_t *instancia)
	{
		if(instancia -> conectada)
		{
			if(instancia -> entradasLibres > espacioLibre)
			{
				retorno = instancia;
				posicionRetorno = posicionActual;
				espacioLibre = instancia -> entradasLibres;
			}
			else if(instancia -> entradasLibres == espacioLibre &&
					posicionRetorno < *puntero && *puntero <= posicionActual) //Si son iguales pregunto por el desempate
			{
				huboDesempate = 1;
				posicionRetorno = posicionActual;
				espacioLibre = instancia -> entradasLibres;
			}
		}
		posicionActual++;
	}

	list_iterate(instancias, buscarCandidatos);

	if(huboDesempate)
		*puntero = posicionRetorno + 1 == list_size(instancias)? 0 : posicionRetorno + 1;

	return retorno;

}

instancia_t *simularDistribucion()
{
	int falsoPuntero = punteroSeleccion;
	return correrAlgoritmo(&falsoPuntero);
}


int enviarEncabezado(socket_t conexion, uint8_t valor) {
	header head;
	head.protocolo = valor;
	return enviarHeader(conexion, head);
}

int enviarResultado(socket_t conexion, enum resultadoEjecucion result) {
	int resultado = enviarEncabezado(conexion, 12);
	resultado += enviarBuffer(conexion, (void*) &result,
			sizeof(enum resultadoEjecucion));
	return resultado < 0;

}

void enviarInfoEntradas(socket_t instancia) {
	cantEntradas_t cantEntradas = config_get_int_value(configuracion,
			"CantEntradas");
	tamEntradas_t tamEntradas = config_get_int_value(configuracion,
			"TamEntradas");
	enviarBuffer(instancia, (void*) &cantEntradas, sizeof(cantEntradas_t));
	enviarBuffer(instancia, (void*) &tamEntradas, sizeof(tamEntradas_t));
}

void removerClave(instancia_t *instancia, char *clave)
{
	bool esClave(void* data)
	{
		return string_equals_ignore_case(clave, (char*) data);
	}
	dictionary_remove(claves, clave);
	list_remove_and_destroy_by_condition(instancia -> claves, esClave, free);
}

char* instrToArray(enum instruccion instr) {
	switch (instr) {
	case get:
		return "GET";
	case set:
		return "SET";
	case store:
		return "STORE";
	case create:
		return "create";
	case compactacion:
		return "compactacion";
	default:
		return "instrucción no valida";
	}
}

void limpiarOperacion() {
	operacion.id = 0;
	operacion.clave = NULL;
	operacion.validez = 0;
	operacion.valor = NULL;
}

void salirConError(char *error) {
	error_show(error);
	exit(1);
}
