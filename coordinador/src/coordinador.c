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
int socketInstanciaMax = 0;

int main(void) {

	inicializacion();

	struct sockaddr_in dirPlanificador;
	socket_t socketPlanificador;
	int esPlanificador;

	socketCoordinador = crearSocketServer (IPEscucha,config_get_string_value(configuracion, Puerto));

	 do {
		socketPlanificador = esperarYaceptar(socketCoordinador ,&dirPlanificador);
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

	//me rindo, podria mandar un mail a los ayudantes por que no se como le tengo que mandar el segundo parametro
	//list_iterate(&listaInstancias,(void *) setearReadfdsInstancia);
	//list_iterate(&listaEsi,(void *) setearReadfdsEsi);

	select(socketInstanciaMax + 1,&readfds,NULL,NULL,NULL);
	// debo revisar si los socket que quedaron en readfds son esi o instancia, si son instancia reviso si quieren compactar o si ya terminaron,
	// si son esi reviso si me envian comandos

}

/*
 * No sé que vas a hacer con estas dos funciones
 * pero me da que es exceso de delegación
 *
 * [MATI]
 */

//las puse por que segun entendi, el list_iterate tiene que recibir una funcion que recibe elementos de la lista
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
	int estadoDellegadaHeader = recibirMensaje(socketAceptado,1,(void *) &header);

	switch(header->protocolo){

	case 2:
		/*
		 * Si el header que te llego dice protocolo = 2 es una instancia que nueva
		 * que se está conectando y no puede ser otra cosa (porque el numero de
		 * protocolo seria otro).
		 * Segun entiendo tu codigo, en está función solo se van a procesar los
		 * mensajes de socket recien creadas (salvo la del planificador) en cuyo
		 * caso, solo deberia de poder llegar los protocolos 2 a 4 inclusive.
		 *
		 * [MATI]
		 */
		registrarInstancia(socketAceptado);
		break;

	case 3:
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
	list_add(listaEsi,&nuevaEsi);
	if (nuevaEsi.socket > socketInstanciaMax) {
		socketInstanciaMax = nuevaEsi.socket;
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
	int * tamMensj = NULL;
	int estadoDellegada;
	Instancia instanciaRecibida;

	do {
		estadoDellegada = recibirMensaje(socket,4,(void *) &tamMensj );
	} while (estadoDellegada);

	char * nombre = NULL;


	do {
			estadoDellegada = recibirMensaje(socket,*tamMensj,(void *) &nombre );
		} while (estadoDellegada);

	strcpy(instanciaRecibida.nombre , nombre);
	instanciaRecibida.idinstancia = cantInstancias;
	instanciaRecibida.socket = socket;

	cantInstancias = list_add(listaInstancias,&instanciaRecibida);


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

	/*
	 * No hay problema si lo dejas así
	 * pero si queres tenerlo en mente
	 * acordate que los valor numericos
	 * cuentan como booleanos en C
	 * (0 = false, diferente de 0 = true)
	 *
	 * [MATI]
	 */
	if (estado != 0)
	{
		error_show ("no se pudo enviar informacion de entradas a la instancia %s",instanciaRecibida.nombre );
	}

	free(buffer);
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
	cantInstancias = 0;
	listaEsi = list_create();
	listaInstancias = list_create();
}
