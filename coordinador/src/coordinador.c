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
t_dictionary * tablaDeClaves = NULL;

socket_t  socketPlanificador;
socket_t  socketCoordinador;

pthread_mutex_t mListaInst;

char * Algoritmo;
int contadorEquitativeLoad = 0;
int cantInstancias = 0;
t_log * logOperaciones;

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
	// aun no la voy a desarrollar, pero esta sera la funcion que maneje el hilo del Planificador
}

void escucharInstancia (Instancia  instancia) {
	header * header;
	int * buffer;
	int enviado;
	int resultado;

		recibirMensaje(instancia.socket,sizeof(header),(void *) &header);

		if(header->protocolo == 12){
			recibirMensaje(instancia.socket,sizeof(int),(void *) &resultado );

			buffer = malloc(sizeof(header) + sizeof(int) );

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

void escucharEsi (Esi esi) {
	header * header;
	enum instruccion * instr = NULL;

		recibirMensaje(esi.socket,sizeof(header),(void *) &header);
		recibirMensaje(esi.socket,sizeof(int),(void *) &esi.idEsi);

		if (header->protocolo == 8){

			recibirMensaje(esi.socket,sizeof(enum instruccion),(void *) &instr );

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

	if( consultarPorClaveTomada(*esi) == 0){

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

		log_info(logOperaciones, "ESI %d STORE %s " , esi->idEsi , esi->clave);

		instancia->esiTrabajando = esi;

		escucharInstancia(*instancia);

	}else{

		error_show("no se puede liberar una clave que nunca fue tomada por el esi. ");

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

	if( consultarPorClaveTomada(*esi) == 0){

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

		escucharInstancia(*instancia);

	}else{
		error_show("la clave no pertenece al esi "  );

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

	int * buffer = malloc(sizeof(header) + sizeof(enum tipoDeInstruccion) + sizeof(int) * 2 + tamClave);
	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &esi.idEsi, sizeof(int) );
	memcpy(buffer+sizeof(header) + sizeof(int) , &tipo, sizeof(enum tipoDeInstruccion) );
	memcpy(buffer+sizeof(header) + sizeof(int) + sizeof(enum tipoDeInstruccion)  , &tamClave, sizeof(int) );
	memcpy(buffer+sizeof(header) + sizeof(int) + sizeof(enum tipoDeInstruccion) + sizeof(int) , &esi.clave, tamClave );


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
		//registrarInstancia es quien verifica que las id sean 0 (instancia nueva) o una id asignada de una instancia que se reconecta
		break;
	case 3:
		registrarEsi(socketAceptado);
		//ahora registrar esi, ademas de registrarla crea un hilo que se encargara de atenderla
		break;
	default:
		break;
	}
}

void registrarEsi(socket_t socket){

	Esi nuevaEsi;
	nuevaEsi.socket = socket;
	nuevaEsi.clave = NULL;
	nuevaEsi.valor = NULL;
	list_add(listaEsi,&nuevaEsi);

	pthread_t hiloEsi;
	pthread_create(&hiloEsi, NULL, (void*) escucharEsi, &nuevaEsi);
}

void registrarInstancia(socket_t socket)
{
	int * tamMensj = NULL;
	int idInstancia;
	Instancia instanciaRecibida;
	instanciaRecibida.esiTrabajando = NULL;
	int enviado;

	char * nombre = NULL;
	recibirMensaje(socket,sizeof(int),(void *) &idInstancia );

	if(idInstancia == 0){

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

	}else{
		//aca esta el caso donde una instancia se reconecta, debo corregir el socket de la version de la instancia que ya tengo en la lista
		//y debo enviar a esa instancia las claves que tenia
	}


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
	pthread_mutex_lock(&mListaInst);
	instancia = list_get(listaInstancias, contadorEquitativeLoad);
	pthread_mutex_unlock(&mListaInst);


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
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n");
	Algoritmo = config_get_string_value(configuracion, "Algoritmo");
	listaEsi = list_create();
	listaInstancias = list_create();
	tablaDeClaves = dictionary_create();

	logOperaciones = log_create("archivoLog.txt","logOperaciones",1,LOG_LEVEL_INFO);

}
