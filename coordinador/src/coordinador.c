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
fd_set readfds;
t_dictionary * tablaDeClaves = NULL;

socket_t  socketPlanificador;
socket_t  socketCoordinador;

pthread_mutex_t mListaInst;

char * Algoritmo;
int contadorEquitativeLoad = 0;
int cantInstancias = 0;
int socketInstanciaMax = 0;
int idInstanciaAuxiliar;
t_log * logOperaciones;

int main(void) {

	struct sockaddr_in dirPlanificador;
	int esPlanificador;
	pthread_t hiloRecibeConexiones;
	pthread_t hiloEscuchaPorAcciones;
	FD_ZERO(&readfds);

	inicializacion();

	socketCoordinador = crearSocketServer (IPEscucha,config_get_string_value(configuracion, Puerto));

	 do {
		socketPlanificador = esperarYaceptar(socketCoordinador ,&dirPlanificador);
		esPlanificador = validarPlanificador(socketPlanificador );
	} while (esPlanificador);

	pthread_create(&hiloRecibeConexiones, NULL, (void*) recibirConexiones, NULL);
	pthread_create(&hiloEscuchaPorAcciones, NULL, (void*) escucharPorAcciones, NULL);

	pthread_join(hiloRecibeConexiones, NULL);
	pthread_join(hiloEscuchaPorAcciones, NULL);

	liberarRecursos();
	exit(0);

}



void escucharPorAcciones () {

	while(1){
		list_iterate(listaInstancias,(void*) setearReadfdsInstancia);
		list_iterate(listaEsi,(void*) setearReadfdsEsi);

		select(socketInstanciaMax + 1,&readfds,NULL,NULL,NULL);

		list_iterate(listaInstancias, (void*) escucharReadfdsInstancia);
		list_iterate(listaEsi, (void*) escucharReadfdsEsi);
	}
}


void escucharReadfdsInstancia (Instancia  instancia) {
	header * header;
	int * buffer;
	int enviado;
	if (FD_ISSET(instancia.socket, &readfds))
	{
		recibirMensaje(instancia.socket,sizeof(header),(void *) &header);

		if(header->protocolo == 12){
			recibirMensaje(instancia.socket,sizeof(int),(void *) &buffer );

			buffer = malloc(sizeof(header) + sizeof(int) );
			int resultado = EXITO;
			memcpy(buffer , &header->protocolo , sizeof(header) );
			memcpy(buffer+sizeof(header) , &resultado , sizeof(int));

			enviado = enviarBuffer (instancia.esiTrabajando->socket , buffer , sizeof(header) + sizeof(int) );

			while(enviado != 0){
				error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
				enviado = enviarBuffer (instancia.esiTrabajando->socket , buffer , sizeof(header) + sizeof(int) );
			}
			free(buffer);
		}else{
			//aca va el codigo que trata los mensajes de la compactacion de Instancias
		}
	}
}

void escucharReadfdsEsi (Esi esi) {
	header * header;
	enum instruccion * instr = NULL;
	int estadoDellegada;

		if (FD_ISSET(esi.socket, &readfds))
		{
			recibirMensaje(esi.socket,sizeof(header),(void *) &header);

			recibirMensaje(esi.socket,sizeof(int),(void *) &esi.idEsi);

			if (header->protocolo == 8){

				do {
					estadoDellegada = recibirMensaje(esi.socket,sizeof(enum instruccion),(void *) &instr );
				} while (estadoDellegada);

				esi.instr = *instr;

				switch(esi.instr){
				case get:
					tratarGet(&esi);
					break;
				case set:
					tratarSet(&esi);
					break;
				case store:
					tratarStore(&esi);
					break;
				}
			}
		}
}

void tratarGet(Esi * esi){
	tamClave_t * tamClave = NULL;
	header header;
	header.protocolo = 15;
	enum tipoDeInstruccion tipo = bloqueante;
	int enviado;

	recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );

	recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

	if( dictionary_has_key(tablaDeClaves, esi->clave) ){

		int * buffer = malloc(sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int) + *tamClave );

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &tipo , sizeof(enum tipoDeInstruccion));
		memcpy(buffer+sizeof(header)+sizeof(enum tipoDeInstruccion) , tamClave , sizeof(int));
		memcpy(buffer+sizeof(header)+sizeof(enum tipoDeInstruccion)+sizeof(int), &esi->clave , *tamClave );

		enviado = enviarBuffer (socketPlanificador , buffer , sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int) + *tamClave );

		while(enviado != 0){
			error_show ("no se envio correctamente el informe de get al planificador, enviando nuevamente");
			enviado = enviarBuffer (socketPlanificador , buffer , sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int) + *tamClave );
		}
		free(buffer);

	}

	else

	{
		//comunico de get a la instancia
		Instancia * instancia = algoritmoUsado();
		dictionary_put(tablaDeClaves, esi->clave, &instancia);
		header.protocolo = 11;
		int * buffer = malloc(sizeof(header) + sizeof(enum instruccion) + sizeof(int) + *tamClave);
		memcpy(buffer,&header.protocolo,sizeof(header));
		memcpy(buffer+sizeof(header),&esi->instr,sizeof(enum instruccion));
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion),tamClave,sizeof(int));
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(int),esi->clave,*tamClave);

		enviado = enviarBuffer(instancia->socket,buffer,sizeof(header) + sizeof(enum instruccion) + sizeof(int) + *tamClave);
		while(enviado !=0 ){
			error_show("no se envio correctamente la clave a la instancia, volviendo a intentar");
			enviado = enviarBuffer(instancia->socket,buffer,sizeof(header) + sizeof(enum instruccion) + sizeof(int) + *tamClave);
			}
		free(buffer);

		//comunico de get al planificador

		buffer = malloc(sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int) + *tamClave );

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &tipo , sizeof(enum tipoDeInstruccion));
		memcpy(buffer+sizeof(header)+sizeof(enum tipoDeInstruccion) , tamClave , sizeof(int));
		memcpy(buffer+sizeof(header)+sizeof(enum tipoDeInstruccion)+sizeof(int), &esi->clave , *tamClave );

		enviado = enviarBuffer (socketPlanificador , buffer , sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int) + *tamClave );

		while(enviado != 0){
			error_show ("no se envio correctamente el informe de get al planificador, enviando nuevamente");
			enviado = enviarBuffer (socketPlanificador , buffer , sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int) + *tamClave );
		}
		free(buffer);

	}
}

void tratarStore(Esi * esi) {
	tamClave_t * tamClave = NULL;
	header header;
	header.protocolo = 11;
	int * buffer;
	int estado;

	recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );

	recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

	if( consultarPorClaveTomada(*esi) == 1){

		Instancia * instancia = dictionary_get(tablaDeClaves, esi->clave);

		buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(int));
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &esi->instr , sizeof(enum instruccion) );
		memcpy(buffer+sizeof(header)+sizeof(int) , tamClave , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(int), &esi->clave , *tamClave );

		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(int) );

		while(estado !=0){
			error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
			estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(int) );
		}
		free(buffer);

		/*
		 * El log tiene que decir que ESI hizo el STORE.
		 * Copio el formato que da el enunciado:
		 * ESI 1 STORE materias:K3001
		 * [MATI]
		 */
		log_info(logOperaciones, "ESI %d STORE %s " , esi->idEsi , esi->clave);

		instancia->esiTrabajando = esi;

	}else{

		error_show("no se puede liberar una clave que nunca fue tomada por el esi. ");
		log_info(logOperaciones,"%s no esta tomada,no se puede hacer STORE, aborto operacion." , esi->clave);

		header.protocolo = 12;

		buffer = malloc(sizeof(header) + sizeof(int) );
		int resultado = ERROR;
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &resultado , sizeof(int));

		estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(int) );

		while(estado != 0){
			error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
			estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(int) );
		}
		free(buffer);

	}

}

void tratarSet(Esi * esi){
	tamClave_t * tamClave = NULL;
	tamValor_t * tamValor = NULL;
	header header;
	header.protocolo = 11;
	int * buffer;
	int estado;

	recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
	recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);
	recibirMensaje(esi->socket,sizeof(tamValor_t),(void *) &tamValor );
	recibirMensaje(esi->socket,*tamValor,(void *) &esi->valor);


	if( consultarPorClaveTomada(*esi) != 0){

		Instancia * instancia = dictionary_get(tablaDeClaves, esi->clave);

		buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(int) * 2);
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &esi->instr , sizeof(enum instruccion) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(int), &esi->clave , *tamClave );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(int)+*tamClave , tamValor , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(int)*2+*tamClave , &esi->valor , *tamValor );


		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(int) * 2);

		while(estado !=0){
			error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
			estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(int) );
			}

		instancia->esiTrabajando = esi;
		free(buffer);
		log_info(logOperaciones, "ESI %d SET %s %s " , esi->idEsi ,esi->clave ,esi->valor);

	}else{
		error_show("la clave no pertenece al esi "  );
		log_info(logOperaciones,"%s no esta tomada, no se puede hacer SET, aborto operacion." , esi->clave);

		header.protocolo = 12;

		buffer = malloc(sizeof(header) + sizeof(int) );
		int resultado = ERROR;
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &resultado , sizeof(int));

		estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(int) );

		while(estado != 0){
			error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
			estado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(int) );
		}
		free(buffer);

	}

}



void setearReadfdsInstancia (Instancia * instancia){
	pthread_mutex_lock(&mListaInst);
	FD_SET(instancia->socket,&readfds);
	pthread_mutex_unlock(&mListaInst);

}

void setearReadfdsEsi (Esi * esi){
	FD_SET(esi->socket,&readfds);
}


int consultarPorClaveTomada(Esi esi){
	header header;
	header.protocolo = 9;
	tamClave_t  tamClave = sizeof(esi.clave);
	enum tipoDeInstruccion tipo;

	if(esi.instr == store){
		tipo = bloqueante;
	} else {
		tipo = noDefinido;
	}

	int * buffer = malloc(sizeof(header) + sizeof(enum tipoDeInstruccion) + sizeof(int) + tamClave);
	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &tipo, sizeof(enum tipoDeInstruccion) );
	memcpy(buffer+sizeof(header) + sizeof(enum tipoDeInstruccion)  , &tamClave, sizeof(int) );
	memcpy(buffer+sizeof(header) + sizeof(enum tipoDeInstruccion) + sizeof(int) , &esi.clave, tamClave );


	int estado = enviarBuffer (socketPlanificador , buffer , sizeof(header) + sizeof(int) + sizeof(int) + tamClave);

	while(estado !=0){
		error_show("no se pudo preguntar por el estado de la clave al planificador, volviendo a intentar");
		estado = enviarBuffer (socketPlanificador , buffer , sizeof(header) + sizeof(int) + sizeof(int) + tamClave);
		}
	free(buffer);

	buffer = malloc(sizeof(int));
	recibirMensaje(socketPlanificador,sizeof(header),(void *) &header.protocolo);
	recibirMensaje(socketPlanificador,sizeof(int),(void *) &buffer );

	if (*buffer == 0){
			free(buffer);
			return 0;
		} else {
			free(buffer);
			return 1;
		}
}

void recibirConexiones() {

	socket_t socketAceptado;
	struct sockaddr_in dirAceptado;

	while (1)
		{
			socketAceptado = esperarYaceptar(socketCoordinador ,&dirAceptado);
			esESIoInstancia(socketAceptado,dirAceptado);
		}
}


void esESIoInstancia (socket_t socketAceptado,struct sockaddr_in dir)
{

	header* header;
	recibirMensaje(socketAceptado,1,(void *) &header);

	switch(header->protocolo){

	case 2:
		registrarInstancia(socketAceptado);
		break;
	case 3:
		registrarEsi(socketAceptado);
		break;
	case 4:
		reconectarInstancia(socketAceptado);
		break;
	default:
		break;
	}
}

void reconectarInstancia(socket_t socket){
	Instancia * instancia;
	int * idInstancia;
	recibirMensaje(socket,sizeof(int),(void *) &idInstancia );

	idInstanciaAuxiliar = *idInstancia;

	pthread_mutex_lock(&mListaInst);
	instancia = list_find(listaInstancias,(void *) idInstancia);
	pthread_mutex_unlock(&mListaInst);


	instancia->socket = socket;

}

bool idInstancia(Instancia * instancia){
	return instancia->idinstancia == idInstanciaAuxiliar;
}

void registrarEsi(socket_t socket){

	Esi nuevaEsi;
	nuevaEsi.socket = socket;
	nuevaEsi.clave = NULL;
	nuevaEsi.valor = NULL;
	list_add(listaEsi,&nuevaEsi);
	if (nuevaEsi.socket > socketInstanciaMax) {
		socketInstanciaMax = nuevaEsi.socket;
	}
}

void registrarInstancia(socket_t socket)
{
	int * tamMensj = NULL;
	Instancia instanciaRecibida;
	instanciaRecibida.esiTrabajando = NULL;
	int enviado;

	char * nombre = NULL;

	recibirMensaje(socket,sizeof(int),(void *) &tamMensj );
	recibirMensaje(socket,*tamMensj,(void *) &nombre );

	strcpy(instanciaRecibida.nombre , nombre);
	instanciaRecibida.idinstancia = cantInstancias + 1;
	instanciaRecibida.socket = socket;

	pthread_mutex_lock(&mListaInst);
	cantInstancias = list_add(listaInstancias,&instanciaRecibida);
	pthread_mutex_unlock(&mListaInst);

	header header;
	header.protocolo = 5;
	cantEntradas_t cantEntradas = config_get_int_value(configuracion, "CantEntradas");
	tamEntradas_t tamEntradas = config_get_int_value(configuracion, "TamEntradas");
	int * buffer = malloc(sizeof(header) + sizeof(instanciaRecibida.idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );


	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &instanciaRecibida.idinstancia , sizeof(int) );
	memcpy(buffer+sizeof(header)+sizeof(int) , &cantEntradas , sizeof(cantEntradas) );
	memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(cantEntradas) , &tamEntradas , sizeof(tamEntradas) );

	enviado = enviarBuffer (instanciaRecibida.socket , buffer , sizeof(header) + sizeof(instanciaRecibida.idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );


	while(enviado != 0){
		error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
		enviado = enviarBuffer (instanciaRecibida.socket , buffer , sizeof(header) + sizeof(instanciaRecibida.idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );
	}

	free(buffer);
}

Instancia * algoritmoUsado(){

	Instancia * instancia;
	if(strcmp("EL",*Algoritmo) == 0){
		instancia = algoritmoEquitativeLoad();
	}/*else if(strcmp("LSU",*Algoritmo) == 0){
		return algoritmoLastStatementUsed();
	}else if(strcmp("KE",*Algoritmo) == 0){
		return algoritmoKeyExplicit();
	} no voy a hacer estos algoritmos para este sabado*/

	return instancia;
}


Instancia * algoritmoEquitativeLoad(){

	Instancia * instancia;
	instancia = list_get(listaInstancias, contadorEquitativeLoad);

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
		return 0;
	}
}

void liberarRecursos()
{

	close(socketCoordinador);
	log_destroy(logOperaciones);

	if(configuracion != NULL)
		config_destroy(configuracion);
}

void salirConError(char * error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}

void inicializacion()
{
	configuracion = config_create(archivoConfig);
	if(configuracion == NULL)
		salirConError("Fallo al leer el archivo de configuracion del coordinador\n");
	Algoritmo = config_get_string_value(configuracion, "Algoritmo");
	listaEsi = list_create();
	listaInstancias = list_create();
	tablaDeClaves = dictionary_create();

	logOperaciones = log_create("archivoLog.txt","logOperaciones",1,LOG_LEVEL_INFO);

}
