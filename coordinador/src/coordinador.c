/*
 ============================================================================
 Name        : coordinador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include "coordinador.h"

t_config * configuracion = NULL;
t_list * listaInstancias = NULL;
t_list * listaEsi = NULL;
t_list * listaClaves = NULL;
t_dictionary * tablaDeClaves = NULL;

socket_t  socketPlanificador;
socket_t  socketCoordinador;

pthread_mutex_t mRespuestaPlanificador;
pthread_cond_t sbRespuestaPlanificador;
pthread_mutex_t mListaInst;
pthread_mutex_t mEsiActual;
pthread_cond_t sbRespuestaInstancia;
pthread_mutex_t mRespuestaInstancia;
pthread_cond_t sbConsultarPorClave;
pthread_mutex_t mConsultarPorClave;

char * Algoritmo;
int auxiliarIdInstancia;
int contadorEquitativeLoad = 0;
int cantInstancias = 0;
int respuestaPlanificador;
t_log * logOperaciones;
Esi * esiActual;

int main(int argc, char **argv) {

	struct sockaddr_in dirPlanificador;
	int esPlanificador;
	pthread_t hiloRecibeConexiones;

	inicializacion(argv[1]);

	socketCoordinador = crearSocketServer (IPEscucha,config_get_string_value(configuracion, Puerto));

	 do {
		socketPlanificador = esperarYaceptar(socketCoordinador ,&dirPlanificador);
		esPlanificador = validarPlanificador(socketPlanificador );
		//validarPlanificador crea el hilo que atendera al planificador
	} while (esPlanificador);

	pthread_create(&hiloRecibeConexiones, NULL, (void*) recibirConexiones, NULL);

	pthread_join(hiloRecibeConexiones, NULL);

	liberarRecursos();
	exit(0);

}

void esucharPlanificador (socket_t socket){
	header header;
	header.protocolo = 9;
	enum tipoDeInstruccion tipo;

	pthread_mutex_lock(&mEsiActual);
	pthread_mutex_lock(&mConsultarPorClave);
	pthread_cond_wait(&sbConsultarPorClave, &mConsultarPorClave);

	tamClave_t tamClave = string_length(esiActual->clave);

	switch(esiActual->instr){
	case store:
		tipo = liberadora;
		break;
	case get:
		tipo = bloqueante;
		break;
	case set:
		tipo = noDefinido;
		break;
	}

	void * buffer = malloc(sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) + tamClave);

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &esiActual->idEsi, sizeof(ESI_id) );
	memcpy(buffer+sizeof(header) + sizeof(ESI_id) , &tipo, sizeof(enum tipoDeInstruccion) );
	memcpy(buffer+sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion)  , &tamClave, sizeof(tamClave_t) );
	memcpy(buffer+sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) , &esiActual->clave, tamClave );

	int estado = enviarBuffer (socketPlanificador , buffer , sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) + tamClave);

	while(estado !=0){
		error_show("no se pudo preguntar por el estado de la clave al planificador, volviendo a intentar");
		estado = enviarBuffer (socketPlanificador , buffer , sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) + tamClave);
		}
	free(buffer);

	int *respuesta = malloc(sizeof(booleano));
	recibirMensaje(socketPlanificador,sizeof(header),(void *) &header.protocolo);
	recibirMensaje(socketPlanificador,sizeof(int),(void *) &respuesta );

	pthread_mutex_lock(&mRespuestaPlanificador);
	respuestaPlanificador = *respuesta;
	pthread_mutex_unlock(&mRespuestaPlanificador);
	pthread_cond_signal(&sbRespuestaPlanificador);

	pthread_mutex_unlock(&mConsultarPorClave);
	pthread_mutex_unlock(&mEsiActual);

	free(respuesta);

}

void escucharInstancia (Instancia * instancia) {
	header * header;
	void * buffer;
	int enviado;
	int * resultado;

	//el mutex es por que el cond lo exije, pero la verdad yo no veo region critica en ese codigo
	//si hay una mejor manera, la esucho
	pthread_mutex_lock(&mRespuestaInstancia);
	pthread_cond_wait(&sbRespuestaInstancia, &mRespuestaInstancia);
		recibirMensaje(instancia->socket,sizeof(header),(void *) &header);

		if(header->protocolo == 12){
			recibirMensaje(instancia->socket,sizeof(enum resultadoEjecucion),(void *) &resultado );

			switch(*resultado){
			case exito:
					buffer = malloc(sizeof(header) + sizeof(enum resultadoEjecucion) );

					memcpy(buffer , &header->protocolo , sizeof(header) );
					memcpy(buffer+sizeof(header) , &resultado , sizeof(enum resultadoEjecucion));

					enviado = enviarBuffer (instancia->esiTrabajando->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );

					while(enviado != 0){
						error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
						enviado = enviarBuffer (instancia->esiTrabajando->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );
					}
					free(buffer);
				break;
			case fallo:
					buffer = malloc(sizeof(header) + sizeof(enum resultadoEjecucion) );

					memcpy(buffer , &header->protocolo , sizeof(header) );
					memcpy(buffer+sizeof(header) , &resultado , sizeof(enum resultadoEjecucion));

					enviado = enviarBuffer (instancia->esiTrabajando->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );

					while(enviado != 0){
						error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
						enviado = enviarBuffer (instancia->esiTrabajando->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );
					}
					free(buffer);
				break;
			case necesitaCompactar:
				// aun no voy a codear este caso
				break;
			}
		}
		pthread_mutex_unlock(&mRespuestaInstancia);
}

void escucharEsi (Esi * esi) {
	header * header;
	enum instruccion * instr = NULL;

	if( !seDesconectoSocket(esi->socket) ){

		recibirMensaje(esi->socket,sizeof(header),(void *) &header);
		recibirMensaje(esi->socket,sizeof(int),(void *) &esi->idEsi);

				if (header->protocolo == 8){

					recibirMensaje(esi->socket,sizeof(enum instruccion),(void *) &instr );

					esi->instr = *instr;

					switch(esi->instr){
					case get:
						free(header);
						free(instr);
						tratarGet(esi);
						break;
					case set:
						free(header);
						free(instr);
						tratarSet(esi);
						break;
					case store:
						free(header);
						free(instr);
						tratarStore(esi);
						break;
					}
				}
	}else{
		error_show("se desconecto socket del esi, no se pudo recibir la instruccion ");
		//puse un error, pero no estoy seguro respecto de cual es el trato indicado para la desconecxion de un esi
	}


}

void tratarGet(Esi * esi){
	tamClave_t * tamClave = NULL;
	header header;
	int enviado;

	recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );

	recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);


	if( dictionary_has_key(tablaDeClaves, esi->clave) ){
		setearEsiActual(*esi);
		pthread_cond_signal(&sbConsultarPorClave);
	}

	else

	{

		Instancia * instancia = algoritmoUsado();
		dictionary_put(tablaDeClaves, esi->clave, &instancia);

		header.protocolo = 11;

		void * buffer = malloc(sizeof(header) + sizeof(enum instruccion) + sizeof(tamClave_t) + *tamClave);
		memcpy(buffer,&header.protocolo,sizeof(header));
		memcpy(buffer+sizeof(header),&esi->instr,sizeof(enum instruccion));
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion),tamClave,sizeof(tamClave_t));
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t),esi->clave,*tamClave);

		enviado = enviarBuffer(instancia->socket,buffer,sizeof(header) + sizeof(enum instruccion) + sizeof(tamClave_t) + *tamClave);

		while(enviado !=0 ){
			error_show("no se envio correctamente la clave a la instancia, volviendo a intentar");
			enviado = enviarBuffer(instancia->socket,buffer,sizeof(header) + sizeof(enum instruccion) + sizeof(tamClave_t) + *tamClave);
			}
		free(buffer);

		setearEsiActual(*esi);
		pthread_cond_signal(&sbConsultarPorClave);

		list_add(listaClaves,esi->clave);
	}
}

void tratarStore(Esi * esi) {
	tamClave_t * tamClave = NULL;
	header header;
	header.protocolo = 11;
	void * buffer;
	int estado;

	recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );

	recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

	setearEsiActual(*esi);
	pthread_cond_signal(&sbConsultarPorClave);

	pthread_cond_wait(&sbRespuestaPlanificador,&mRespuestaPlanificador);

	pthread_mutex_lock(&mRespuestaPlanificador);

	if( respuestaPlanificador ){

		pthread_mutex_unlock(&mRespuestaPlanificador);

		Instancia * instancia = dictionary_get(tablaDeClaves, esi->clave);

		buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t));
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &esi->instr , sizeof(enum instruccion) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(tamClave_t) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t), &esi->clave , *tamClave );

		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) );

		while(estado !=0){
			error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
			estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) );
		}
		free(buffer);

		log_info(logOperaciones, "ESI %d STORE %s " , esi->idEsi , esi->clave);

		instancia->esiTrabajando = esi;

		pthread_cond_signal(&sbRespuestaInstancia);

	}else{
		pthread_mutex_unlock(&mRespuestaPlanificador);

		header.protocolo = 12;

		buffer = malloc(sizeof(header) + sizeof(booleano) );
		booleano resultado = ERROR;
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &resultado , sizeof(booleano));

		estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(booleano) );

		while(estado != 0){
			error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
			estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(booleano) );
		}
		free(buffer);

	}

}

void tratarSet(Esi * esi){
	tamClave_t * tamClave = NULL;
	tamValor_t * tamValor = NULL;
	header header;
	header.protocolo = 11;
	void * buffer;
	int estado;

	recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
	recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);
	recibirMensaje(esi->socket,sizeof(tamValor_t),(void *) &tamValor );
	recibirMensaje(esi->socket,*tamValor,(void *) &esi->valor);

	setearEsiActual(*esi);
	pthread_cond_signal(&sbConsultarPorClave);

	pthread_cond_wait(&sbRespuestaPlanificador,&mRespuestaPlanificador);

	pthread_mutex_lock(&mRespuestaPlanificador);

	if( respuestaPlanificador ){

		pthread_mutex_unlock(&mRespuestaPlanificador);

		Instancia * instancia = dictionary_get(tablaDeClaves, esi->clave);

		buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(tamClave_t) + sizeof(tamValor_t));
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &esi->instr , sizeof(enum instruccion) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(tamClave_t) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t), &esi->clave , *tamClave );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t)+*tamClave , tamValor , sizeof(tamValor_t) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t)+*tamClave+sizeof(tamValor_t), &esi->valor , *tamValor );


		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(tamClave_t) + sizeof(tamValor_t));

		while(estado !=0){
			error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
			estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) + sizeof(tamValor_t) );
			}

		instancia->esiTrabajando = esi;
		free(buffer);
		log_info(logOperaciones, "ESI %d SET %s %s " , esi->idEsi ,esi->clave ,esi->valor);

		pthread_cond_signal(&sbRespuestaInstancia);

	}else{
		pthread_mutex_unlock(&mRespuestaPlanificador);

		error_show("la clave no pertenece al esi "  );

		header.protocolo = 12;

		buffer = malloc(sizeof(header) + sizeof(booleano) );
		booleano resultado = ERROR;
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &resultado , sizeof(booleano));

		estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(booleano) );

		while(estado != 0){
			error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
			estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(booleano) );
		}
		free(buffer);

	}

}

void setearEsiActual(Esi esi){
	pthread_mutex_lock(&mEsiActual);
	esiActual->instr = esi.instr;
	esiActual->idEsi = esi.idEsi;
	esiActual->socket = esi.socket;
	strcpy(esiActual->clave,esi.clave);
	pthread_mutex_unlock(&mEsiActual);
}

/*
 * Acordate que todas las conexiones son hilos separadados,
 * incluida la del planificador por lo tanto no vas a llamar
 * una función sino sincronizarte con el hilo del planificador
 * para hacer la consulta.
 */

void recibirConexiones() {

	socket_t socketAceptado;
	struct sockaddr_in dirAceptado;

	while (1)
		{
			socketAceptado = esperarYaceptar(socketCoordinador ,&dirAceptado);
			esESIoInstancia(socketAceptado);
		}
}

void esESIoInstancia (socket_t socketAceptado)
{

	header* Header;
	recibirMensaje(socketAceptado,sizeof(header),(void *) &Header);

	switch(Header->protocolo){
	case 2:
		registrarInstancia(socketAceptado);
		break;
	case 3:
		registrarEsi(socketAceptado);
		break;
	default:
		break;
	}
}

void registrarEsi(socket_t socket){

	Esi * nuevaEsi = malloc(sizeof(Esi));
	nuevaEsi->socket = socket;
	nuevaEsi->clave = NULL;
	nuevaEsi->valor = NULL;
	list_add(listaEsi,nuevaEsi);

	pthread_t hiloEsi;
	pthread_create(&hiloEsi, NULL, (void*) escucharEsi, nuevaEsi);
	//TODO hacer free en algun lugar
}

void registrarInstancia(socket_t socket)
{
	tamNombreInstancia_t * tamNombre = NULL;
	instancia_id * idInstancia;
	Instancia * instanciaRecibida = malloc(sizeof(Instancia));
	instanciaRecibida->esiTrabajando = NULL;
	int enviado;
	pthread_t hiloInstancia;

	char * nombre = NULL;
	recibirMensaje(socket,sizeof(int),(void*) &idInstancia );

	if(*idInstancia == 0){

		recibirMensaje(socket,sizeof(int),(void *) &tamNombre );
		recibirMensaje(socket,*tamNombre,(void *) &nombre );

		//¿ese strcpy no tiene problema con pasarle el puntero "nombre" ?
		strcpy(instanciaRecibida->nombre , nombre);
		instanciaRecibida->idinstancia = cantInstancias + 1;
		instanciaRecibida->socket = socket;

		pthread_mutex_lock(&mListaInst);
		cantInstancias = list_add(listaInstancias,instanciaRecibida);
		pthread_mutex_unlock(&mListaInst);

		header header;
		header.protocolo = 5;
		cantEntradas_t cantEntradas = config_get_int_value(configuracion, "CantEntradas");
		tamEntradas_t tamEntradas = config_get_int_value(configuracion, "TamEntradas");

		int * buffer = malloc(sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) );

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &instanciaRecibida->idinstancia , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(int) , &cantEntradas , sizeof(cantEntradas_t) );
		memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(cantEntradas_t) , &tamEntradas , sizeof(tamEntradas_t) );

		enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) );


		while(enviado != 0){
			error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
			enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) );
		}
		//TODO hacer free en algun lugar de la instancia
		free(buffer);

		pthread_create(&hiloInstancia, NULL, (void*) escucharInstancia, instanciaRecibida);

	}else{

		auxiliarIdInstancia = *idInstancia;
		instanciaRecibida = list_find(listaInstancias,(void *) buscarInstanciaPorId);

		instanciaRecibida->socket = socket;

		header header;
		header.protocolo = 4;

		cantEntradas_t cantEntradas = config_get_int_value(configuracion, "CantEntradas");
		tamEntradas_t tamEntradas = config_get_int_value(configuracion, "TamEntradas");

		t_list * listaDeClaves = list_filter(listaClaves,(void*) buscarClaves );

		int i;
		tamClave_t tamClave;
		cantClaves_t cantClaves = list_size(listaDeClaves);
		char * claveAux;

		int * buffer = malloc(sizeof(header) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) + sizeof(cantClaves_t));

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &cantEntradas , sizeof(cantEntradas_t) );
		memcpy(buffer+sizeof(header)+sizeof(cantEntradas_t) , &tamEntradas , sizeof(tamEntradas_t) );
		memcpy(buffer+sizeof(header)+sizeof(cantEntradas_t)+sizeof(tamEntradas_t) , &cantClaves , sizeof(cantClaves_t) );

		enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) + sizeof(cantClaves_t));

		while(enviado != 0){
			error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
			enviado = enviarBuffer (instanciaRecibida->socket , buffer ,sizeof(header) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) + sizeof(cantClaves_t));
		}

		free(buffer);

		for(i=0; i<cantClaves ;i++){

			claveAux = list_get(listaDeClaves, i);
			tamClave = string_length(claveAux);

			buffer = malloc(tamClave + sizeof(tamClave_t) );

			memcpy(buffer, &tamClave , sizeof(tamClave_t) );
			memcpy(buffer, claveAux, tamClave );

			enviado = enviarBuffer (instanciaRecibida->socket , buffer , tamClave + sizeof(tamClave_t) );

			while(enviado != 0){
				error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
				enviado = enviarBuffer (instanciaRecibida->socket , buffer , tamClave + sizeof(tamClave_t) );
			}
			free(buffer);
		}

	}
}

int buscarClaves(char * clave){
	Instancia * instancia = dictionary_get( tablaDeClaves, clave);
	if(instancia->idinstancia ==  auxiliarIdInstancia)
		return 1;
	else
		return 0;

}

int buscarInstanciaPorId(Instancia * instancia){
	if(instancia->idinstancia == auxiliarIdInstancia)
		return 1;
	else
		return 0;

}

Instancia * algoritmoUsado(void){

	Instancia * instancia;
	if(strcmp("EL", Algoritmo) == 0){
		instancia = algoritmoEquitativeLoad();
	}/*else if(strcmp("LSU",*Algoritmo) == 0){
		return algoritmoLastStatementUsed();
	}else if(strcmp("KE",*Algoritmo) == 0){
		return algoritmoKeyExplicit();
	} no voy a hacer estos algoritmos para este sabado*/

	return instancia;
}

Instancia * algoritmoEquitativeLoad(void){

	Instancia * instancia;
	pthread_mutex_lock(&mListaInst);
	instancia = list_get(listaInstancias, contadorEquitativeLoad);
	pthread_mutex_unlock(&mListaInst);

	/*
	 * ¿Qué pasa si el contador está en 8 con 9 instancia
	 * y la siguiente vez que se llama al algoritmo se
	 * desconectaron dos instancias?
	 * ¿Qué pasa si reinicio el contador pero antes de volver
	 * a llamar al algoritmo se conectaron más instancias?
	 * [MATI]
	 */
	if(contadorEquitativeLoad == cantInstancias){
		contadorEquitativeLoad = 0;
	}else{
		contadorEquitativeLoad++;
	}

	return instancia;
}

int esperarYaceptar(socket_t socketCoordinador,struct sockaddr_in* dir)
{
	unsigned int tam;
	listen(socketCoordinador , 5);
	int socket = accept(socketCoordinador, (void*)&dir, &tam);
	return socket;
	}

int validarPlanificador (socket_t socket) {
	header* handshake = NULL;
	int estadoDellegada;
	pthread_t hiloPlanificador;

	estadoDellegada = recibirMensaje(socket,sizeof(header),(void*) &handshake);
	if (estadoDellegada != 0)
	{
		error_show ("no se pudo recibir header de planificador.");
		return 1;
	}

	if (handshake->protocolo != 1)
	{
		close(socket);
		return 1;
	}
	else
	{
		pthread_create(&hiloPlanificador, NULL, (void*) esucharPlanificador, &socket);
		return 0;
	}
}

void liberarRecursos()
{

	close(socketCoordinador);
	log_destroy(logOperaciones);

	if(configuracion != NULL)
		config_destroy(configuracion);
	pthread_mutex_destroy (&mListaInst);
}

void salirConError(char * error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}

int crearConfiguracion(char* archivoConfig){
 if(!configuracion)
	configuracion = config_create(archivoConfig);
 return configuracion? 0 : 1;
}

 void inicializacion (char* dirConfig)
	{
	if(crearConfiguracion(dirConfig))
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n ");
	Algoritmo = config_get_string_value(configuracion, "Algoritmo");
	listaEsi = list_create();
	listaInstancias = list_create();
	listaClaves = list_create();
	tablaDeClaves = dictionary_create();

	pthread_mutex_init (&mListaInst, NULL);

	pthread_mutex_init(&mEsiActual,NULL);

	pthread_mutex_init (&mRespuestaInstancia, NULL);
	pthread_cond_init (&sbRespuestaInstancia, NULL);

	pthread_mutex_init (&mRespuestaPlanificador,NULL);
	pthread_cond_init (&sbRespuestaPlanificador,NULL);

	pthread_mutex_init (&mConsultarPorClave,NULL);
	pthread_cond_init (&sbConsultarPorClave,NULL);

	logOperaciones = log_create("archivoLog.txt","logOperaciones",1,LOG_LEVEL_INFO);

}
