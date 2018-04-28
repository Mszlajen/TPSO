/*
 ============================================================================
 Name        : instancia.c
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

void inicializarSockaddr (struct sockaddr_in *direccion, int puerto){
 int servidor;
	direccion->sin_family = AF_INET;
	direccion->sin_addr.s_addr = INADDR_ANY;
	direccion->sin_port = htons(puerto);
	servidor = socket(AF_INET, SOCK_STREAM, 0);
	if (bind(servidor, (void*) &direccion, sizeof(direccion)) != 0){
		perror("Falló el bind");
		return;
	}

	printf("Estoy escuchando\n");
	listen(servidor,100);
 }

void conectarSocket (int socketCoordinador,struct sockaddr_in direccion){
	if( bind (socketCoordinador, (void*)&direccion , sizeof(struct sockaddr_in) ) != 0){
			perror("No se pudo asociar el puerto, revisar bind\n");
		}
}

int main(void) {
	struct sockaddr_in direccion;
	int puerto = 8080;
	int socketCoordinador = socket(AF_INET,SOCK_STREAM,0);

	inicializarSockaddr(&direccion, puerto);
	conectarSocket(socketCoordinador, direccion);

		return EXIT_SUCCESS;
}
