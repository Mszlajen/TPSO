#include "biblio.h"

void salir_agraciadamente (int vSalida)
{
	exit(vSalida);
}

int crearSocketCliente(char * IP, char * puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(IP, puerto, &hints, &server_info);

	int sock = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

	int resultado = connect(sock, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	if(resultado == ERROR)
	{
		salir_agraciadamente(1);
	}
	return sock;
}

FILE * abrirArchivoLectura (char * dirr)
{
	FILE * archivo = fopen(dirr, "r");
	if(archivo == NULL)
		salir_agraciadamente(1);
	return archivo;
}

void enviarHeader (int sock, header head)
{
	int tamHeader = sizeof(header);
	void * buffer = malloc(tamHeader);
	memcpy(buffer, (void*) &head, tamHeader);
	int bytesEnviados = send(sock, buffer, tamHeader, 0);
	if(bytesEnviados == ERROR)
		salir_agraciadamente(1);
	/*
	 * Lo que sigue es un control porque send
	 * puede llegar a no enviar todo lo pedido
	 * (En este caso es tan chico que no deberia
	 * haber problema pero mejor prevenir que lamentar)
	 */
	int resultSend;
	while(bytesEnviados != tamHeader)
	{
		if((resultSend = send(sock, buffer + bytesEnviados, tamHeader - bytesEnviados, 0)) == ERROR)
			salir_agraciadamente(1);
		bytesEnviados += resultSend;
	}

	free(buffer);
}
