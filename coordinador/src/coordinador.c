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

pthread_mutex_t mListaInst;

char * Algoritmo;
int auxiliarIdInstancia;
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
	/*
	 * ¿Por qué buffer de int si no le estás metiendo int?
	 * Y resultado no es un int, es un tipo de dato definido en
	 * la biblioteca compartida.
	 * [MATI]
	 */
	int * buffer;
	int enviado;
	int resultado;

		recibirMensaje(instancia.socket,sizeof(header),(void *) &header);

		/*
		 * La compactación es un resultado de ejecución no otro numero de mensaje.
		 * Me habia olvidado de ponerlo en el enum.
		 * [MATI]
		 */
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

		/*
		 * ¿Decime que es ese int?
		 * Recibir mensaje deja una dirección a memoria dinamica en el
		 * tercer parametro, por eso es void**, no copia el valor en la variable.
		 * ¿Qué pasa si el ESI se desconecto?
		 * y, ¿Para que mantener todas las demas variables del struct
		 * en memoria si solo las usas una vez que el ESI envio un mensaje?
		 * [MATI]
		 */
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
	/*
	 * ¿Para qué darle valor nueve al header si el
	 * unico lugar donde lo usas lo reasignas a once?
	 */
	header header;
	header.protocolo = 9;
	int enviado;

	recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );

	recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

	if( dictionary_has_key(tablaDeClaves, esi->clave) ){

		consultarPorClaveTomada(*esi);
		// al consultar por la clave, el planificador sabe que se realizo una operacion bloqueante de tal clave en tal esi

	}

	else

	{

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

		consultarPorClaveTomada(*esi);
		// al consultar por la clave, el planificador sabe que se realizo una operacion bloqueante de tal clave en tal esi

		list_add(listaClaves,esi->clave);
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
		/*
		 * Mostrar por pantalla es importante pero no cuando tenes que loggear en un
		 * archivo y la función que lo hace tambien te muestra por pantalla.
		 * [MATI]
		 */
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
/*
 * Acordate que todas las conexiones son hilos separadados,
 * incluida la del planificador por lo tanto no vas a llamar
 * una función sino sincronizarte con el hilo del planificador
 * para hacer la consulta.
 */
int consultarPorClaveTomada(Esi esi){
	header header;
	header.protocolo = 9;
	/*
	 * Estás guardando el tamaño de un puntero, no el tamaño de la
	 * clave, para eso tenes que recordarlo vos o usar una función
	 * de strings
	 * [MATI]
	 */
	tamClave_t  tamClave = sizeof(esi.clave);
	enum tipoDeInstruccion tipo;
	/*
	 * Te falta un tipo de instrucción y las que están, están mal
	 * [MATI]
	 */
	if(esi.instr == store){
		tipo = bloqueante;
	} else {
		tipo = noDefinido;
	}
	/*
	 * Seguis con los int
	 * [MATI]
	 */
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
	int * tamMensj = NULL;
	int idInstancia;
	Instancia * instanciaRecibida = malloc(sizeof(Instancia));
	instanciaRecibida->esiTrabajando = NULL;
	int enviado;

	char * nombre = NULL;
	recibirMensaje(socket,sizeof(int),(void*) &idInstancia );

	if(idInstancia == 0){

		recibirMensaje(socket,sizeof(int),(void *) &tamMensj );
		recibirMensaje(socket,*tamMensj,(void *) &nombre );

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
		int * buffer = malloc(sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &instanciaRecibida->idinstancia , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(int) , &cantEntradas , sizeof(cantEntradas) );
		memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(cantEntradas) , &tamEntradas , sizeof(tamEntradas) );

		enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );


		while(enviado != 0){
			error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
			enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );
		}
		//TODO hacer free en algun lugar de la instancia
		free(buffer);

	}else{

		auxiliarIdInstancia = idInstancia;
		instanciaRecibida = list_find(listaInstancias,(void *) buscarInstanciaPorId);

		instanciaRecibida->socket = socket;

		header header;
		header.protocolo = 4;
		cantEntradas_t cantEntradas = config_get_int_value(configuracion, "CantEntradas");
		tamEntradas_t tamEntradas = config_get_int_value(configuracion, "TamEntradas");

		t_list * listaDeClaves = list_filter(listaClaves,(void*) buscarClaves );

		int i,tamTodasClaves = 0,cantClaves = list_size(listaDeClaves),tamClave;
		char * claveAux;

		for(i=0; i<cantClaves ;i++){
			claveAux = list_get(listaDeClaves, i);
			tamTodasClaves += sizeof(claveAux);
		}

		int * buffer = malloc(sizeof(header) + sizeof(cantEntradas) + sizeof(tamEntradas) + sizeof(int)*2 + tamTodasClaves );

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &cantEntradas , sizeof(cantEntradas) );
		memcpy(buffer+sizeof(header)+sizeof(cantEntradas) , &tamEntradas , sizeof(tamEntradas) );
		memcpy(buffer+sizeof(header)+sizeof(cantEntradas)+sizeof(tamEntradas) , &cantClaves , sizeof(int) );

		int tamanioClaveIteracion = 0, tamanioIntIteracion = 0;

		for(i=0; i<cantClaves ;i++){
			claveAux = list_get(listaDeClaves, i);
			tamClave = sizeof(claveAux);

			memcpy(buffer+sizeof(header)+sizeof(cantEntradas)+sizeof(tamEntradas)+sizeof(cantClaves) + tamanioClaveIteracion + tamanioIntIteracion, &tamClave , sizeof(int) );
			memcpy(buffer+sizeof(header)+sizeof(cantEntradas)+sizeof(tamEntradas)+sizeof(cantClaves)+sizeof(tamClave)+ tamanioClaveIteracion + tamanioIntIteracion, claveAux, tamClave );

			tamanioClaveIteracion += sizeof(claveAux);
			tamanioIntIteracion += sizeof(tamClave);
		}

		enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );


		while(enviado != 0){
			error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
			enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );
		}
	}
}

int buscarClaves(char * clave){
	Instancia * instancia = dictionary_get( tablaDeClaves, clave);
	if(instancia->idinstancia ==  auxiliarIdInstancia){
		return 1;
	}else{
		return 0;
	}
}

int buscarInstanciaPorId(Instancia * instancia){
	if(instancia->idinstancia == auxiliarIdInstancia){
		return 1;
	}else{
		return 0;
	}

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

	logOperaciones = log_create("archivoLog.txt","logOperaciones",1,LOG_LEVEL_INFO);

}
