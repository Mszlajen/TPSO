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

socket_t  socketCoordinador;

int cantInstancias;

int main(void) {

	inicializacion();

	struct sockaddr_in dirPlanificador;
	socket_t socketPlanificador;
	int esPlanificador;

	socketCoordinador = crearSocketServer (IPEscucha,config_get_string_value(configuracion, Puerto));

	 do {
		socketPlanificador = esperarYaceptar(socketCoordinador, 2 ,&dirPlanificador);
		esPlanificador = validarPlanificador(socketPlanificador );
	} while (esPlanificador);


	 pthread_t hiloRecibeConexiones;
	 //pthread_create(&hiloRecibeConexiones, NULL, (void*) recibirConexiones, NULL);

	 FD_ZERO(&readfds);

	 pthread_t hiloEscuchaPorAcciones;
	 //pthread_create(&hiloEscuchaPorAcciones, NULL, (void*) escucharPorAcciones, NULL);

	//close(socketCoordinador);
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

	struct timeval tv;

	tv.tv_sec = 2;
	tv.tv_usec = 500000;
	int ultimosock;
	// debo inicializar ultimosock, con el socket de mayor valor (valor como int) que esta en la lista de Esi->socket, no se como hacerlo... xD

	//list_iterate(&listaInstancias,setearReadfdsInstancia);
	//list_iterate(&listaEsi,setearReadfdsEsi);

	//como le mando la clausura al list iterate?

	select(ultimosock,&readfds,NULL,NULL,&tv);
	// debo revisar si los socket que quedaron en readfds son eso o instancia, si son instancia reviso si quieren compactar o si ya terminaron,
	// si son esi reviso si me envian comandos

}

void setearReadfdsInstancia (Instancia instancia){
	FD_SET(instancia.socket,&readfds);
}

void setearReadfdsEsi (Esi esi){
	FD_SET(esi.socket,&readfds);
}

void recibirConexiones() {

	socket_t socketAceptado;
	struct sockaddr_in dirAceptado;

	while (1)
		{
			socketAceptado = esperarYaceptar(socketCoordinador, 20 ,&dirAceptado);
			esESIoInstancia(socketAceptado,dirAceptado);
		}
}

void esESIoInstancia (socket_t socketAceptado,struct sockaddr_in dir)
{

	header* header = malloc(sizeof(header));
	/*
	 * El warning ya les dice que le están pasando un int**
	 * cuando espera un void** así que por lo menos les falta
	 * el casteo. Ahora, si la idea es recibir el header
	 * ¿por qué no usar el tipo header que está definido en
	 * la biblioteca compartida?
	 *
	 * [MATI]
	 */
	int estadoDellegadaHeader = recibirMensaje(socketAceptado,1,(void *) header->protocolo);

	switch(header->protocolo){

	case 2:
		// hay que crear un modulo que revise si la instancia esta registrada o no y atienda respecto de este dato
		// si no esta registrada, se registra, si lo esta y me habla es o por que termino de ejecutar o por que necesita compactar
		free(header);
		registrarInstancia(socketAceptado);
		break;

	case 3:
		free(header);
		registrarEsi(socketAceptado);
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


void registrarEsi(socket_t socket){

	Esi nuevaEsi;
	nuevaEsi.socket = socket;

	if (listaEsi == NULL)
		{
			listaEsi = list_create();
			list_add(listaEsi,&nuevaEsi);
		}
	else
		{
			list_add(listaEsi,&nuevaEsi);
		}
	/*
	 *
	 * [MATI]
	 *
	 * PD: Antes de que sigan este camino los invito
	 * a que vean esto piensen si no hay otra manera
	 * de resolverlo que crear un hilo por ESI.
	 * https://github.com/sisoputnfrba/foro/issues/1012
	 */
}

void registrarInstancia(socket_t socket)
{
	int tamMensj;
	int estadoDellegada;
	Instancia instanciaRecibida;

	do {
		estadoDellegada = recibirMensaje(socket,4,(void *) &tamMensj );
	} while (estadoDellegada);

	char * nombre = malloc(tamMensj);


	do {
			estadoDellegada = recibirMensaje(socket,tamMensj,(void *) nombre );
		} while (estadoDellegada);

	strcpy(instanciaRecibida.nombre , nombre);
	instanciaRecibida.idinstancia = cantInstancias;
	instanciaRecibida.socket = socket;

	free(nombre);

	if (listaInstancias == NULL)
		{
			listaInstancias = list_create();
			cantInstancias = list_add(listaInstancias,&instanciaRecibida);
		}
	else
		{
			cantInstancias = list_add(listaInstancias,&instanciaRecibida);
		}

	header header;
	header.protocolo = 5;
	int cantEntradas = config_get_int_value(configuracion, "CantEntradas");
	int tamEntradas = config_get_int_value(configuracion, "TamEntradas");
	int * buffer = malloc(sizeof(header) + sizeof(instanciaRecibida.idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );


	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &instanciaRecibida.idinstancia , sizeof(int) );
	memcpy(buffer+sizeof(int) , &cantEntradas , sizeof(cantEntradas) );
	memcpy(buffer+sizeof(cantEntradas) , &tamEntradas , sizeof(tamEntradas) );

	int estado = enviarBuffer (instanciaRecibida.socket , buffer , sizeof(header) + sizeof(instanciaRecibida.idinstancia) + sizeof(cantEntradas) + sizeof(tamEntradas) );


	if (estado != 0)
	{
		error_show ("no se pudo enviar informacion de entradas a la instancia %s",instanciaRecibida.nombre );
	}

	free(buffer);
}


int esperarYaceptar(socket_t socketCoordinador, int colaMax,struct sockaddr_in* dir)
{
	unsigned int tam;
	listen(socketCoordinador , colaMax);
	int socket = accept(socketCoordinador, (void*)&dir, &tam);
	return socket;
	}

int validarPlanificador (socket_t socket) {
	header* handshake = NULL;
	int estadoDellegada;
	uint8_t correcto = 1;

	estadoDellegada = recibirMensaje(socket,sizeof(header),&handshake);
	if (estadoDellegada != 0)
	{
		error_show ("no se pudo recibir header de planificador.");
		return 1;
	}

	if (handshake->protocolo != correcto)
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
	cantInstancias = 0;

}




