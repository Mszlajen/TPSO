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
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>

void hiloDeInstancia(int socket){
char* buffer = malloc(5));
int mensajeRecibido = recv(socket,buffer,4,0);

free(buffer);
}
void esESIoInstancia (int socketAceptado) {
	//aqui ira la logica que reconozca segun el header si es un ESI o una Instancia
	//en caso de ser una Instancia, debere ir a otro modulo que haga send de los tamaños
	//y cree un hilo que se quede haciendo listen
char* buffer = malloc(5))//Cuando tengamos definido el tamaño hay que cambiarlo
int mensajeRecibido = recv(socketAceptado,buffer,4,0);


//if (mensajeRecibido==)
//{
pthread_t hiloInstancias;
pthread_created(&hiloInstancias,20,(void*)hiloDeInstancia,socketAceptado);//El segundo argumento del created es para dar espacio de memoria al hilo
//}
//else{}
//HAY QUE PONER EN ALGUN LADO UN CLOSE DEL HILO!!!

}

// me gustaria que revisaran este modulo, por que no recuerdo bien como era lo de los punteros y podria estar cometiendo errores
void inicializarSockaddr (struct sockaddr_in *direccion) // debe recibir el puerto, por ahora no lo agregamos
{
	direccion->sin_addr.s_addr = htons(INADDR_ANY);
	direccion->sin_family = AF_INET;
	direccion->sin_port = htons(8080); // aca va el puerto que lee del archivo puse 8080 a modo de ejemplo
	memset(&(direccion->sin_zero),'\0',8);
}

void conectarSocket (int socketCoordinador,struct sockaddr_in direccion)
{
	if( bind (socketCoordinador, (void*)&direccion , sizeof(struct sockaddr_in) ) != 0)
		{
			perror("no se pudo asociar puerto, revisar bind");
		}
}

int main(void) {

	//previo a esto, se leyo el archivo de configuracion

	struct sockaddr_in direccion,dirPlanificador,dirAceptado;
	int socketPlanificador, socketAceptado,socketCoordinador = socket(AF_INET,SOCK_STREAM,0);
	unsigned int tamaDeSockaddr;

	inicializarSockaddr (&direccion);
	conectarSocket(socketCoordinador, direccion);

	listen(socketCoordinador , 2); // espero Planificador
	socketPlanificador = accept(socketCoordinador, (void*)&dirPlanificador, &tamaDeSockaddr);

	while (0) {
		listen(socketCoordinador , 20);
		socketAceptado = accept(socketCoordinador, (void*)&dirAceptado, &tamaDeSockaddr);
		esESIoInstancia(socketAceptado);
	}


	close(socketCoordinador); // lo dejo para no olvidarmelo

	return EXIT_SUCCESS;
}
