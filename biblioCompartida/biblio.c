#include "biblio.h"

void salir_agraciadamente (int vSalida)
{
	exit(vSalida);
}

socket_t crearSocketCliente(char * IP, char * puerto)
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
		return resultado;
	return sock;
}

socket_t crearSocketServer(char * IP, char * puerto)
{
	struct addrinfo hints, *res;
	int sock;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(IP, puerto, &hints, &res);

	sock = socket(res -> ai_family, res -> ai_socktype, res -> ai_protocol);

	int si = 1;
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&si,sizeof(si)) == ERROR)
	{
		perror("setsockopt");
		salir_agraciadamente(1);
	}

	int resultado = bind(sock, res-> ai_addr, res->ai_addrlen);

	if(resultado == ERROR)
	{
		close(sock);
		perror("");
		return resultado;
	}
	return sock;
}


socket_t crearSocketClientePorFD(socket_t fd, char *ip)
{
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	getsockname(fd, (struct sockaddr *)&addr, &addr_size);
	char *puerto = string_from_format("%i", ntohs(addr.sin_port));
	socket_t sock = crearSocketCliente(ip, puerto);
	free(puerto);
	return sock;
}

int seDesconectoSocket(socket_t sock)
{
	void * buffer = malloc(1);
	int resultado = recv(sock, buffer, 1, (MSG_PEEK | MSG_DONTWAIT));
	free(buffer);
	if(resultado == 0)
		return 1;
	else
		return 0;
}

int enviarHeader (socket_t sock, header head)
{
	int resultado = enviarBuffer(sock, &head, sizeof(header));
	return resultado;
}

int enviarBuffer (socket_t sock, void* buffer, int tamBuffer)
{
	int bytesEnviados = send(sock, buffer, tamBuffer, 0);
	if(bytesEnviados == ERROR)
		return ERROR;
		/*
		 * Lo que sigue es un control porque send
		 * puede llegar a no enviar todo lo pedido
		 * (En nuestro caso es poco probable que
		 * lo necesite porque son mensajes chicos
		 * pero mejor prevenir que lamentar)
		 * [Lo mismo aplica a la recepcion]
		 */
	int resultSend;
	while(bytesEnviados != tamBuffer)
	{
		if((resultSend = send(sock, buffer + bytesEnviados, tamBuffer - bytesEnviados, 0)) == ERROR)
			return ERROR;
		bytesEnviados += resultSend;
	}
	return EXITO;
}

int recibirMensaje(socket_t sock, int tamMens, void** buffer)
{
	*buffer = malloc(tamMens);
	int bytesRecibidos = recv(sock, *buffer, tamMens, 0);
	if(bytesRecibidos == ERROR)
		return ERROR;
	if(!bytesRecibidos)
		return 1;
	int resultRecv;
	while(bytesRecibidos != tamMens)
	{
		if((resultRecv = recv(sock, *buffer + bytesRecibidos, tamMens - bytesRecibidos, 0)) == ERROR)
			return ERROR;
		if(!resultRecv)
			return 1;
		bytesRecibidos += resultRecv;
	}
	return EXITO;
}

socket_t aceptarConexion (socket_t socketServer)
{
	struct sockaddr_storage infoDirr;
	socklen_t size_infoDirr = sizeof(struct sockaddr_storage);

	return accept(socketServer, (struct sockaddr *) &infoDirr, &size_infoDirr);
}

void cerrarSocket(socket_t sock)
{
	if(sock == ERROR)
		close(sock);
}
