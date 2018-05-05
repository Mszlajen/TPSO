/*
 ============================================================================
 Name        : coordinador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <biblio.h>
#include <string.h>
#include <commons/config.h>
#include <commons/collections/list.h>


#define IPEscucha "127.0.0.2"

#define archivoConfig "coordinador.config"
#define Puerto "Puerto"

t_config * configuracion = NULL;



void hiloDeInstancia()
{
	printf ("soy el hilo, hola!");
}

void esESIoInstancia (int socketAceptado,struct sockaddr_in dir)
{

	int* mensaje;
	int estadoDellegada = recibirMensaje(socketAceptado,4,&mensaje);

	/*
	aqui ira la logica que reconozca segun el header si es un ESI o una
	Instancia en caso de ser una Instancia, debere ir a el modulo hiloDeInstancia,
	que hara send de los tamaños y esperara una respuesta.
	*/

	if (mensaje[0]==2)
	{
		pthread_t hiloInstancia;
		//pthread_create(&hiloInstancia, NULL, (void*) hiloDeInstancia, NULL);
	}

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

void inicializacion ()
{
	configuracion = config_create(archivoConfig);
	if(configuracion == NULL)
		salirConError("Fallo al leer el archivo de configuracion del coordinador\n");

}

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


