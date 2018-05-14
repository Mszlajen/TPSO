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
int cantInstancias;
int cantEsi;

int main(void) {

	inicializacion();

	struct sockaddr_in dirPlanificador,dirAceptado;
	int socketPlanificador, socketAceptado,socketCoordinador;
	int esPlanificador = 1;

	socketCoordinador = crearSocketServer (IPEscucha,config_get_string_value(configuracion, Puerto));


	while (esPlanificador) {
		socketPlanificador = esperarYaceptar(socketCoordinador, 2 ,&dirPlanificador);
		esPlanificador = validarPlanificador(socketPlanificador );
	}



	while (1)
	{
		socketAceptado = esperarYaceptar(socketCoordinador, 20 ,&dirAceptado);
		esESIoInstancia(socketAceptado,dirAceptado);
	}
	//close(socketCoordinador);
	exit(0);
}


void esESIoInstancia (int socketAceptado,struct sockaddr_in dir)
{

	int* mensaje;
	int estadoDellegada = recibirMensaje(socketAceptado,4,&mensaje);

	switch(mensaje[0]){

	case 2:
		registrarInstancia(socketAceptado,mensaje);
		break;

	case 3:

		// aca ira la accion a realizarse en el caso de ser una ESI
		break;

	default:

		//Creo que no hace nada si no es Esi o Instancia
		break;

	}
}

void atenderEsi(int socket){

}

void registrarEsi(int socket){

	Esi nuevaEsi;
	nuevaEsi.socket = socket;
	pthread_t hiloEsi;
	nuevaEsi.hiloEsi = hiloEsi;

	if (listaEsi == NULL)
		{
			listaEsi = list_create();
		}
	else
		{
			cantEsi = list_add(listaEsi,&nuevaEsi);
		}
	pthread_create(&nuevaEsi.hiloEsi, NULL, (void*) atenderEsi, socket);
}

void registrarInstancia(int socket,int* mensaje)
{

	Instancia instanciaRecibida;
	instanciaRecibida.socket = socket;
	memcpy(&instanciaRecibida.nombre,(&mensaje) + 5,(&mensaje)+1);
	instanciaRecibida.idinstancia =  cantInstancias;

	if (listaInstancias == NULL)
		{
			listaInstancias = list_create();
		}
	else
		{
			cantInstancias = list_add(listaInstancias,&instanciaRecibida);
		}

	uint8_t protocolo = 5;
	int copiar;
	int * buffer = malloc(13);

	memcpy(buffer,&protocolo,1);

	copiar = instanciaRecibida.idinstancia;
	memcpy(buffer+1,&copiar,4);

	copiar = config_get_int_value(configuracion, "CantEntradas");
	memcpy(buffer+5,&copiar,4);

	copiar = config_get_int_value(configuracion, "TamEntradas");
	memcpy(buffer+9,&copiar,4);

	int estado = enviarBuffer (instanciaRecibida.socket, buffer, 13);

	if (estado != 0) {printf ("no se pudo enviar informacion de entradas a la instancia %s",instanciaRecibida.nombre ); }
}

int esperarYaceptar(int socketCoordinador, int colaMax,struct sockaddr_in* dir)
{
	unsigned int tam;
	listen(socketCoordinador , colaMax);
	int socket = accept(socketCoordinador, (void*)&dir, &tam);
	return socket;
	}

int validarPlanificador (int socket) {
	header* handshake = NULL;
	int estadoDellegada;
	uint8_t correcto = 1;

	estadoDellegada = recibirMensaje(socket,sizeof(header),&handshake);

	if (estadoDellegada != 0){ printf ("no se pudo recivir header de planificador."); return 1; }
	if (handshake->protocolo != correcto){ return 1; } else {return 0;}
}


void liberarRecursos()
{
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




