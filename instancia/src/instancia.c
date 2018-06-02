#include "instancia.h"

char** tablaDeEntradas = NULL;
t_list* infoClaves = NULL;
infoEntrada* tablaDeControl = NULL;
instancia_id ID = ERROR;
tamEntradas_t tamanioEntradas;
cantEntradas_t cantidadEntradas;
socket_t socketCoord = ERROR;

int main(int argc, char** argv) {
	inicializar(argv[0]);

	pthread_t hiloProcesamiento;
	pthread_create(&hiloProcesamiento, NULL, (void*) procesamientoInstrucciones, NULL);
	pthread_t hiloDump;
	pthread_create(&hiloDump, NULL, (void*) dump, NULL);

	pthread_join(hiloProcesamiento, NULL);
	pthread_join(hiloDump, NULL);

	exit(0);
}

void inicializar(char* dirConfig)
{
	if(crearConfiguracion(dirConfig))
		salirConError("No se pudo abrir la configuracion.");
	infoClaves = list_create();
	conectarConCoordinador();
	recibirRespuestaHandshake();
	tablaDeEntradas = malloc(cantidadEntradas * tamanioEntradas);
	tablaDeControl = malloc(sizeof(infoEntrada) * cantidadEntradas);
}

void dump()
{

}

void procesamientoInstrucciones()
{

}

void conectarConCoordinador()
{
	header head;
	FILE* archivoID = NULL;
	char* dirArchivoID = obtenerDireccionArchivoID();
	void* buffer = NULL;

	socketCoord = crearSocketCliente(obtenerIPCordinador(), obtenerPuertoCoordinador());
	if(socketCoord == ERROR)
		salirConError("No se pudo conectar con el coordinador");

	if((archivoID = fopen(dirArchivoID, "r")))
	{
		fread((void*)&ID, sizeof(instancia_id), 1, archivoID);
		buffer = malloc(sizeof(ID) + sizeof(header));
		head.protocolo = 4;
		memcpy(buffer, &head, sizeof(head));
		memcpy(buffer + sizeof(head), &ID, sizeof(ID));
		enviarBuffer(socketCoord, buffer, sizeof(sizeof(ID) + sizeof(header)));
		fclose(archivoID);
	}
	else
	{

		tamNombreInstancia_t tamNombre = sizeof(char) * string_length(obtenerNombreInstancia()) +
						sizeof(char);
		int tamBuffer = sizeof(head) +
						sizeof(tamNombreInstancia_t) +
						tamNombre;

		buffer = malloc(tamBuffer);
		head.protocolo = 2;
		memcpy(buffer, &head, sizeof(head));
		memcpy(buffer + sizeof(head), &tamNombre, sizeof(tamNombreInstancia_t));
		memcpy(buffer + sizeof(head) + sizeof(tamNombre), obtenerNombreInstancia(), tamNombre);
		enviarBuffer(socketCoord, buffer, tamBuffer);
	}
	free(buffer);
	free(dirArchivoID);
}

void recibirRespuestaHandshake()
{
	void * buffer = NULL;
	int tamMensaje = sizeof(header) +
					 sizeof(instancia_id) +
					 sizeof(tamEntradas_t) +
					 sizeof(cantEntradas_t);
	recibirMensaje(socketCoord, tamMensaje, &buffer);
	header head;
	memcpy(&head, buffer, sizeof(header));
	if(ID == ERROR)
	{
		memcpy(&ID, buffer + sizeof(header), sizeof(instancia_id));
	}
	memcpy(&tamanioEntradas, buffer + sizeof(header) + sizeof(instancia_id), sizeof(tamEntradas_t));
	memcpy(&cantidadEntradas, buffer + sizeof(header) + sizeof(instancia_id) + sizeof(tamEntradas_t), sizeof(cantEntradas_t));
	if(head.protocolo != 5)
	{ /* ERROR */ }
	free(buffer);
}

void almacenarID()
{
	FILE* archivo = NULL;
	char* dirArchivo = obtenerDireccionArchivoID();
	if((archivo = fopen(dirArchivo, "w")))
	{
		fwrite((void*)&ID, sizeof(instancia_id), 1, archivo);
		fclose(archivo);
	}
	free(dirArchivo);
}

void salirConError(char* error)
{

}
