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

	inicializacion();

	socketCoordinador = crearSocketServer (IPEscucha,config_get_string_value(configuracion, Puerto));

	 do {
		socketPlanificador = esperarYaceptar(socketCoordinador ,&dirPlanificador);
		esPlanificador = validarPlanificador(socketPlanificador );
	} while (esPlanificador);



	 pthread_create(&hiloRecibeConexiones, NULL, (void*) recibirConexiones, NULL);

	 FD_ZERO(&readfds);

	 pthread_create(&hiloEscuchaPorAcciones, NULL, (void*) escucharPorAcciones, NULL);

	//close(socketCoordinador);
	//log_destroy(logOperaciones);
	exit(0);
	/*
	 * Comentario:
	 * Ese exit(0) está al pedo porque no deberia llegar
	 * en ejecución normal (ni en anormal por el while(1)
	 * pero si llegara a ser utilizable tendrian que
	 * reemplazarlo con una función que libere los recursos
	 * y despues haga exit(0), como la de error pero sin el
	 * error.
	 *
	 * [MATI]
	 */
}



void escucharPorAcciones () {

	while(1){
		list_iterate(listaInstancias,(void*) setearReadfdsInstancia);
		list_iterate(listaEsi,(void*) setearReadfdsEsi);

		select(socketInstanciaMax + 1,&readfds,NULL,NULL,NULL);
		// debo revisar si los socket que quedaron en readfds son esi o instancia, si son instancia reviso si quieren compactar o si ya terminaron,
		// si son esi reviso si me envian comandos

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
		// tengo que revisar si quieren compactar o si ya terminaron
		if(header->protocolo == 12){
			// debo enviar un mensaje a la esi correspondiente que se ejecuto la instruccion
			recibirMensaje(instancia.socket,sizeof(int),(void *) &buffer );
			//que yo sepa no existe el caso en que la instancia no pueda terminar de ejecutar, salvo por un pedido de compactacion
			//pero eso no se contempla en este bloque, de modo que solo recibo el mensaje para quitar los bits

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

		/*
		 * ¿No puse alguna vez ya que tantos int no se entiende que se supone que son?
		 * Si no lo dije, lo digo ahora.
		 * Si son cosas que ya están definidas en el protocolo, usen eso.
		 * Si no están definidas pueden hacer un typedef exclusivo de su modulo.
		 * Así como está ahora es indebuggeable (y sospecho que está mal)
		 * [MATI]
		 */
		buffer = malloc(sizeof(header) + sizeof(int) + *tamClave + sizeof(int));
		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &esi->instr , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(int) , tamClave , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(int), &esi->clave , *tamClave );

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
		log_info(logOperaciones, "STORE %s " , esi->clave);

		instancia->esiTrabajando = esi;


	}else{
		/*
		 * ¿Para que describir el problema si los errores
		 * tienen nombres definidos en el enunciado?
		 */
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
		log_info(logOperaciones, "SET %s %s ",esi->clave ,esi->valor);

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

/*
 * Esto es exceso de delegación.
 * Escribis exactamente la misma cantidad de caracteres
 * llamando a la funcion y no te aporta nada.
 * [MATI]
 */
// lo hago por que segun entiendo, el list_iterate espera una clausura que recibe instancias y hace algo
// FD_SET segun entiendo recibe mas de un parametro, por eso no puedo mandar a FD_SET como la clausura que espera el iterate
// y entonces es que hago esta delegacion para que sirva como un adaptador entre lo que quiero y lo que espera el list_iterate

/*
 * Entonces está mal definida porque las funciones de interación
 * reciben un puntero al objeto (que es lo que guardan las listas)
 * [MATI]
 */
// pero lo unico que yo necesito poner en el readfds es el socket, osea, no voy a modificar lo elementos de la lista
// solo voy a poner todos los socket en el readfds entonces es lo mismo que reciba la dir de memoria de la instancia, o una copia

void setearReadfdsInstancia (Instancia instancia){
	FD_SET(instancia.socket,&readfds);
}

void setearReadfdsEsi (Esi esi){
	FD_SET(esi.socket,&readfds);
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
	memcpy(buffer+sizeof(header)+sizeof(enum tipoDeInstruccion)  , &tamClave, sizeof(int) );
	memcpy(buffer+sizeof(header)+sizeof(enum tipoDeInstruccion)+sizeof(int)  , &esi.clave, tamClave );


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

	/*
	 * Voy a hacer otra explicación de como funciona recibirMensaje:
	 * 1- Creo una variable puntero del tipo (tipo* p) de lo que quiero
	 * 	  	recibir y no le asigno nada (o NULL a lo sumo)
	 * 2- Le paso como tercer parametro la dirección de ese puntero (&p)
	 * 2.5- Si el tipo del puntero no es un void*, cosa probable, lo
	 * 		casteo a un puntero de puntero de void (void**)
	 * 3- Despues del llamado de la función, si termino bien, el puntero
	 * 		va a dirigir un espacio de memoria donde está el dato
	 * 		recibido.
	 *
	 * [MATI]
	 */
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

		//Creo que no hace nada si no es Esi o Instancia
		/*
		 * No deberia pasar y no creo que lo prueben pero
		 * por ser correctos, si al socket de recepción en
		 * este punto le llega algo que no es ESI o Instancia
		 * podes cerrar ese socket y mandar un mensaje de
		 * error informando al usuario.
		 *
		 * [MATI]
		 */
		break;

	}
}

void reconectarInstancia(socket_t socket){
	Instancia * instancia;
	int * idInstancia;
	recibirMensaje(socket,sizeof(int),(void *) &idInstancia );

	idInstanciaAuxiliar = *idInstancia;

	instancia = list_find(listaInstancias,(void *) idInstancia);

	instancia->socket = socket;

}

bool idInstancia(Instancia instancia){
	/*
	 * ¿Por qué no hacer directamente return instancia.idinstancia == idInstanciaAuxiliar?
	 * [MATI]
	 */
	if(instancia.idinstancia == idInstanciaAuxiliar){
		return 1;
	}else{
		return 0;
	}
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

	cantInstancias = list_add(listaInstancias,&instanciaRecibida);

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
	/*
	 * Consejo:
	 * 	Cada vez que agreguen un recurso, vengan
	 * 	y ya dejan su liberación escrita.
	 *
	 * [MATI]
	 */
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
	/*
	 * Copio la descripcion de log_create:
	 * Crea una instancia de logger, tomando por parametro
	 * el nombre del programa (segundo parametro), el nombre del archivo donde se van a generar los logs (primer parametro),
	 * el nivel de detalle minimo a loguear (cuarto parametro, un enum) y si además se muestra por pantalla lo que se loguea (tercer parametro).
	 */
	logOperaciones = log_create("archivoLog.txt","logOperaciones",1,LOG_LEVEL_INFO);

}
