/*
 ============================================================================
 Name        : coordinador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <biblio.h>
#include <string.h>

#define tamanioDeStackDeInstancia 20
#define IPEscucha "127.0.0.2"

void hiloDeInstancia(int socket)
{

}

void esESIoInstancia (int socketAceptado,struct sockaddr_in dir)
{

	char* mensaje = malloc(5);//Cuando tengamos definido el tamaño hay que cambiarlo
	int estadoDeLlegada = recibirMensaje(socketAceptado,4,&mensaje);

	/*
	aqui ira la logica que reconozca segun el header si es un ESI o una
	Instancia en caso de ser una Instancia, debere ir a el modulo hiloDeInstancia,
	que hara send de los tamaños y esperara una respuesta.
	*/


	if (mensaje[0]==2)
	{
		pthread_t hiloInstancia;
		pthread_create(&hiloInstancia,
					tamanioDeStackDeInstancia,
					hiloDeInstancia,
					(void*)socketAceptado);
		pthread_destroy(hiloInstancia); //asi cierro el hilo, lo deje de referencia
	}

}

int esperarYaceptar(int socketCoordinador, int colaMax,struct sockaddr_in* dir)
{
	unsigned int tam;
	listen(socketCoordinador , colaMax);
	int socket = accept(socketCoordinador, (void*)&dir, &tam);
	return socket;
}

void main(void) {

	//previo a esto, se leyo el archivo de configuracion

	struct sockaddr_in dirPlanificador,dirAceptado;
	int socketPlanificador, socketAceptado,socketCoordinador;

	// el primer parametro es un ip, y el segundo es un puerto
	// ambos deben venir de archivo de conf.

	socketCoordinador = crearSocketServer (IPEscucha,"8000");
	socketPlanificador = esperarYaceptar(socketCoordinador, 2 ,&dirPlanificador);

	while (0)
	{
		socketAceptado = esperarYaceptar(socketCoordinador, 20 ,&dirAceptado);
		esESIoInstancia(socketAceptado,dirAceptado);
	}
	//close(socketCoordinador);
}
