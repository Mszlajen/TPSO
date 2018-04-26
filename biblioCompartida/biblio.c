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
		error_show("fallo la creación del socket para <%s> puerto <%s>", IP, puerto);
		salir_agraciadamente(1);
	}
	return sock;
}

int crearSocketServer(char * IP, char * puerto)
{
		struct addrinfo hints, *res;
		int sock;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		getaddrinfo(IP, puerto, &hints, &res);

		sock = socket(res -> ai_family, res -> ai_socktype, res -> ai_protocol);

		usarPuertoTapado(sock);

		int resultado = bind(sock, res-> ai_addr, res->ai_addrlen);

		if(resultado == -1)
		{
			error_show("fallo la creacion del socket server para <%s> puerto <%s>", IP, puerto);
			salir_agraciadamente(1);
		}
		return sock;
}

void usarPuertoTapado (int sock)
{
	int si = 1;
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&si,sizeof si) == ERROR)
	{
		perror("setsockopt");
		salir_agraciadamente(1);
	}

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
	memcpy(buffer, &head, tamHeader);
	enviarBuffer(sock, buffer, tamHeader);
	free(buffer);
}

void enviarBuffer (int sock, void* buffer, int tamBuffer)
{
	if(tamBuffer == 0)
		tamBuffer = sizeof(*buffer);
	int bytesEnviados = send(sock, buffer, tamBuffer, 0);
	if(bytesEnviados == ERROR)
		salir_agraciadamente(1);
		/*
		 * Lo que sigue es un control porque send
		 * puede llegar a no enviar todo lo pedido
		 * (En este caso es tan chico que no deberia
		 * haber problema pero mejor prevenir que lamentar)
		 */
	int resultSend;
	while(bytesEnviados != tamBuffer)
	{
		if((resultSend = send(sock, buffer + bytesEnviados, tamBuffer - bytesEnviados, 0)) == ERROR)
			salir_agraciadamente(1);
		bytesEnviados += resultSend;
	}
}
