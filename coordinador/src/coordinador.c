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

/*
 * Primero hago está aclaración, por lo que encontre de pthread_cond
 * son más parecidos a los monitores que a los semaforos.
 * Segundo, me parece que no estás entendiendo bien como usarlos, ahora
 * mismo todos los hilos de instancias se quedan esperando por el mismo
 * cond y no hay manera de decirle a la instancia particular que devolvio
 * el algoritmo que se desbloquee (El cond te sirve para esto, pero no de
 * está manera).
 * Tambien me parece que hay recursos sincronizados de más pero no estoy seguro.
 * [MATI]
 */
pthread_mutex_t mRespuestaPlanificador;
pthread_cond_t sbRespuestaPlanificador;
pthread_mutex_t mListaInst;
pthread_mutex_t mEsiActual;
pthread_cond_t sbEsiActual;
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

void hiloPlanificador (socket_t socket){
	header header;
	header.protocolo = 9;
	int *respuesta;
	enum tipoDeInstruccion tipo;

	while(1){

	/*
	 * Acá vas a tener un problema que está sincronización no te va a salvar
	 * de que el mensaje para el comando status te puede llegar en cualquier
	 * momento y vos tendrias que estár escuchando por ese mensaje constantemente
	 * ademas de que habiamos quedado que atendiamos las desconexiones apenas
	 * pasan.
	 */
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

	respuesta = malloc(sizeof(booleano));
	recibirMensaje(socketPlanificador,sizeof(header),(void *) &header.protocolo);
	recibirMensaje(socketPlanificador,sizeof(int),(void *) &respuesta );

		pthread_mutex_lock(&mRespuestaPlanificador);
		respuestaPlanificador = *respuesta;
		pthread_mutex_unlock(&mRespuestaPlanificador);

		pthread_cond_signal(&sbRespuestaPlanificador);

	pthread_mutex_unlock(&mConsultarPorClave);
	free(respuesta);

	}
}

void hiloInstancia (Instancia * instancia) {

	while(1){

	pthread_cond_wait(&sbEsiActual,&mEsiActual);

	switch(instancia->esiTrabajando->instr){
		case get:
			tratarGetInstancia(instancia);
			break;
		case set:
			tratarSetInstancia(instancia);
			break;
		case store:
			tratarStoreInstancia(instancia);
			break;
		}

	pthread_mutex_lock(&mRespuestaInstancia);
	recibirResultadoInstancia(instancia);
	pthread_mutex_unlock(&mRespuestaInstancia);
	}
}


void hiloEsi (Esi * esi) {
	tamClave_t * tamClave = NULL;
	tamValor_t * tamValor = NULL;
	header * header;
	enum instruccion * instr = NULL;

	while(1){
	/*
	 * Esto está mal, lo hace ahora es:
	 * -entrar en el while 1
	 * -comprobar que el ESI está conectado EN ESE INSTANTE
	 * -[Suponiendo que justo no se desconecto] bloquear el hilo hasta que haya un cambio en el socket
	 * -Seguir ejecutando sin saber si ese cambio fue que llego un mensaje o que se desconecto el socket
	 * Para solucionarlo, tenes que poner un listen antes del if para que bloquee
	 * hasta haber un cambio o poner el primer recibirMensaje en el if y consultar
	 * su valor de retorno.
	 * [MATI]
	 */
	if( !seDesconectoSocket(esi->socket) ){

		recibirMensaje(esi->socket,sizeof(header),(void *) &header);
		recibirMensaje(esi->socket,sizeof(int),(void *) &esi->idEsi);

		if (header->protocolo == 8){
				recibirMensaje(esi->socket,sizeof(enum instruccion),(void *) &instr );
				esi->instr = *instr;

				switch(esi->instr){
						case get:
							recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
							recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

							tratarGetEsi(esi);
							enviarResultadoEsi(esi);
							break;
						case set:
							recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
							recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);
							recibirMensaje(esi->socket,sizeof(tamValor_t),(void *) &tamValor );
							recibirMensaje(esi->socket,*tamValor,(void *) &esi->valor);

							tratarSetStoreEsi(esi);
							pthread_cond_wait(&sbRespuestaInstancia,&mRespuestaInstancia);
							enviarResultadoEsi(esi);
							break;
						case store:
							recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
							recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

							tratarSetStoreEsi(esi);
							pthread_cond_wait(&sbRespuestaInstancia,&mRespuestaInstancia);
							enviarResultadoEsi(esi);
							break;
						}
				free(header);
				free(instr);
			}
		}else{
			error_show("se desconecto socket del esi, no se pudo recibir la instruccion ");
			//puse un error, pero no estoy seguro respecto de cual es el trato indicado para la desconexion de un esi
			/*
			 * Sí, va un error y te olvidas que ese ESI existe (o sea eliminas la información relacionada).
			 * [MATI]
			 */
		}
	}
}

/*
 * Si entiendo bien, esto envia la clave a la instancia cuando recibe un get, no?
 * Porque eso está mal, un get no llega a las instancias recien cuando se hace
 * set se crea la información de la clave en la instancia.
 */
void tratarGetInstancia(Instancia * instancia){
	tamClave_t * tamClave = NULL;
	header header;
	int enviado;

		header.protocolo = 11;

		void * buffer = malloc(sizeof(header) + sizeof(enum instruccion) + sizeof(tamClave_t) + *tamClave);

		memcpy(buffer,&header.protocolo,sizeof(header));
		memcpy(buffer+sizeof(header),&instancia->esiTrabajando->instr,sizeof(enum instruccion));
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion),tamClave,sizeof(tamClave_t));
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t),instancia->esiTrabajando->clave,*tamClave);

		/*
		 * Acá podes usar directamente el resultado de enviarBuffer en el while para evitar repetir codigo
		 * (pero esto no está mal, solo tiene cosas de más)
		 * [MATI]
		 */
		enviado = enviarBuffer(instancia->socket,buffer,sizeof(header) + sizeof(enum instruccion) + sizeof(tamClave_t) + *tamClave);

		while(enviado !=0 ){
			error_show("no se envio correctamente la clave a la instancia, volviendo a intentar");
			enviado = enviarBuffer(instancia->socket,buffer,sizeof(header) + sizeof(enum instruccion) + sizeof(tamClave_t) + *tamClave);
			}
		free(buffer);

}


void tratarGetEsi(Esi * esi){

	if( dictionary_has_key(tablaDeClaves, esi->clave) ){

		pthread_mutex_lock(&mEsiActual);
		esiActual = esi;
		pthread_cond_signal(&sbConsultarPorClave);

		pthread_cond_wait(&sbRespuestaPlanificador,&mRespuestaPlanificador);

		if(respuestaPlanificador){
			esi->res = bloqueo;
		}else{
			esi->res = exito;
		}
		pthread_cond_signal(&sbEsiActual);
		pthread_mutex_unlock(&mEsiActual);
	}
	else
	{
		/*
		 * Acá puede llegar a ser al reves, primero consulta
		 * que el set sea valido y despues asigna la instancia
		 * eso dependera de los test.
		 */
		Instancia * instancia = algoritmoUsado();
		dictionary_put(tablaDeClaves, esi->clave, &instancia);

		pthread_mutex_lock(&mEsiActual);
		esiActual = esi;
		pthread_cond_signal(&sbConsultarPorClave);
		pthread_cond_wait(&sbRespuestaPlanificador,&mRespuestaPlanificador);

		if(respuestaPlanificador){
			esi->res = bloqueo;
		}else{
			esi->res = exito;
		}

		list_add(listaClaves,esi->clave);
		instancia->esiTrabajando = esi;

		pthread_cond_signal(&sbEsiActual);
		pthread_mutex_unlock(&mEsiActual);

	}
}


void tratarStoreInstancia(Instancia * instancia) {
	tamClave_t * tamClave = NULL;
	header header;
	header.protocolo = 11;
	void * buffer;
	int estado;

	buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t));

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &instancia->esiTrabajando->instr , sizeof(enum instruccion) );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(tamClave_t) );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t), &instancia->esiTrabajando->clave , *tamClave );

	estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) );

	while(estado !=0){
		error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) );
	}
	free(buffer);
	/*
	 * El %d en formato es para double, te va funcionar pero te va a imprimir los id con coma
	 * [MATI]
	 */
	log_info(logOperaciones, "ESI %d STORE %s " , instancia->esiTrabajando->idEsi , instancia->esiTrabajando->clave);
}

void tratarSetStoreEsi(Esi * esi) {
	Instancia * instancia;

	pthread_mutex_lock(&mEsiActual);
	esiActual = esi;
	pthread_cond_signal(&sbConsultarPorClave);

	pthread_cond_wait(&sbRespuestaPlanificador,&mRespuestaPlanificador);

	if(respuestaPlanificador){
		esi->res = fallo;
	}else{
		instancia = dictionary_get(tablaDeClaves, esi->clave);
		instancia->esiTrabajando = esi;
		pthread_cond_signal(&sbEsiActual);
	}
	pthread_mutex_unlock(&mEsiActual);
}

void tratarSetInstancia(Instancia * instancia){
	tamClave_t * tamClave = NULL;
	tamValor_t * tamValor = NULL;
	header header;
	header.protocolo = 11;
	void * buffer;
	int estado;

	buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(tamClave_t) + sizeof(tamValor_t));

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &instancia->esiTrabajando->instr , sizeof(enum instruccion) );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(tamClave_t) );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t), &instancia->esiTrabajando->clave , *tamClave );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t)+*tamClave , tamValor , sizeof(tamValor_t) );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t)+*tamClave+sizeof(tamValor_t), &instancia->esiTrabajando->valor , *tamValor );

	estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(tamClave_t) + sizeof(tamValor_t));

	while(estado !=0){
		error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) + sizeof(tamValor_t) );
		}

	free(buffer);

	log_info(logOperaciones, "ESI %d SET %s %s " , instancia->esiTrabajando->idEsi ,instancia->esiTrabajando->clave ,instancia->esiTrabajando->valor);
}


void recibirResultadoInstancia(Instancia * instancia){
	header * header;
	int * resultado;

	recibirMensaje(instancia->socket,sizeof(header),(void *) &header);

	if(header->protocolo == 12){
		recibirMensaje(instancia->socket,sizeof(enum resultadoEjecucion),(void *) &resultado );

		switch(*resultado){
		case exito:
			instancia->esiTrabajando->res = exito;
			pthread_cond_signal(&sbRespuestaInstancia);
			break;
		case fallo:
			instancia->esiTrabajando->res = fallo;
			pthread_cond_signal(&sbRespuestaInstancia);
			break;
		case necesitaCompactar:
			// aun no voy a codear este caso
			break;
		}
	}
	free(resultado);
}

void enviarResultadoEsi(Esi * esi){

	header header;
	header.protocolo = 12;
	void * buffer;
	int enviado;

	buffer = malloc(sizeof(header) + sizeof(enum resultadoEjecucion) );

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &esi->res , sizeof(enum resultadoEjecucion));

	enviado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );

	while(enviado != 0){
		error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
		enviado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );
	}
	free(buffer);
}

void setearEsiActual(Esi esi){
	pthread_mutex_lock(&mEsiActual);
	esiActual->instr = esi.instr;
	esiActual->idEsi = esi.idEsi;
	esiActual->socket = esi.socket;
	strcpy(esiActual->clave,esi.clave);
	pthread_mutex_unlock(&mEsiActual);
}

void recibirConexiones() {

	socket_t socketAceptado;
	struct sockaddr_in dirAceptado; //¿Esto para qué lo queres? [MATI]

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
	pthread_create(&hiloEsi, NULL, (void*) hiloEsi, nuevaEsi);
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

		pthread_create(&hiloInstancia, NULL, (void*) hiloInstancia, instanciaRecibida);

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
	do{
		instancia = list_get(listaInstancias, contadorEquitativeLoad);

		if(contadorEquitativeLoad == cantInstancias){
				contadorEquitativeLoad = 0;
			}else{
				contadorEquitativeLoad++;
			}

	}while( seDesconectoSocket( instancia->socket) );
	pthread_mutex_unlock(&mListaInst);

	/*
	 * ¿Qué pasa si el contador está en 8 con 9 instancia
	 * y la siguiente vez que se llama al algoritmo se
	 * desconectaron dos instancias?
	 * ¿Qué pasa si reinicio el contador pero antes de volver
	 * a llamar al algoritmo se conectaron más instancias?
	 * [MATI]
	 *
	 * Esto sigue mal, así que lo voy a decir de otra manera:
	 * El valor de las listas puede cambiar entre llamado y llamado por
	 * lo que la comprobación de que el puntero este en un valor valido
	 * tendria que ser antes de usar el algoritmo no despues.
	 * Y así como está ahora no vas a poder implementar la respuesta de
	 * status porque el llamar a la función que cambia el estado del
	 * puntero y con status eso no deberia pasar.
	 * [MATI]
	 */
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
		pthread_create(&hiloPlanificador, NULL, (void*) hiloPlanificador, &socket);
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
	pthread_cond_init (&sbEsiActual, NULL);

	pthread_mutex_init (&mRespuestaInstancia, NULL);
	pthread_cond_init (&sbRespuestaInstancia, NULL);

	pthread_mutex_init (&mRespuestaPlanificador,NULL);
	pthread_cond_init (&sbRespuestaPlanificador,NULL);

	pthread_mutex_init (&mConsultarPorClave,NULL);
	pthread_cond_init (&sbConsultarPorClave,NULL);

	logOperaciones = log_create("archivoLog.txt","logOperaciones",1,LOG_LEVEL_INFO);

}
