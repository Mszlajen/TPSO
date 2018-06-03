#include "instancia.h"

char* tablaDeEntradas = NULL;
t_dictionary* infoClaves = NULL;
infoEntrada* tablaDeControl = NULL;
instancia_id ID = ERROR;
tamEntradas_t tamanioEntradas;
cantEntradas_t cantidadEntradas, punteroReemplazo = 0;
socket_t socketCoord = ERROR;

int main(int argc, char** argv) {
	inicializar(argv[0]);

	pthread_t hiloProcesamiento;
	pthread_create(&hiloProcesamiento, NULL, (void*) procesamientoInstrucciones, NULL);
	/*pthread_t hiloDump;
	pthread_create(&hiloDump, NULL, (void*) dump, NULL);*/

	pthread_join(hiloProcesamiento, NULL);
	//pthread_join(hiloDump, NULL);

	exit(0);
}

void inicializar(char* dirConfig)
{
	if(crearConfiguracion(dirConfig))
		salirConError("No se pudo abrir la configuracion.");
	infoClaves = dictionary_create();
	conectarConCoordinador();
	recibirRespuestaHandshake();
	tablaDeEntradas = malloc(cantidadEntradas * tamanioEntradas);
	tablaDeControl = malloc(sizeof(infoEntrada) * cantidadEntradas);
	int i;
	for(i = 0; i < cantidadEntradas; i++)
	{
		tablaDeControl[i].clave = NULL;
		tablaDeControl[i].tiempoUltimoUso = 0;
	}
}

void dump()
{

}

void procesamientoInstrucciones()
{
	instruccion_t* instruc;
	while(1)
	{
		instruc = recibirInstruccionCoordinador();
		switch(instruc -> tipo)
		{
		case set:
			if(obtenerAlgoritmoReemplazo() == LRU)
				incrementarUltimoUsoEntradas();
			instruccionSet(instruc);
			break;
		case store:
			if(obtenerAlgoritmoReemplazo() == LRU)
				incrementarUltimoUsoEntradas();
			instruccionStore(instruc);
			break;
		case compactacion:
			break;
		default:
			/*
			 * No me gustan los warnings al pepe.
			 */
			break;
		}
		free(instruc -> clave);
		free(instruc -> valor);
		free(instruc);
	}
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

instruccion_t* recibirInstruccionCoordinador()
{
	listen(socketCoord, 5);
	instruccion_t *instruccion = malloc(sizeof(instruccion_t));

	header* encabezado;
	recibirMensaje(socketCoord, sizeof(header), (void**) &encabezado);
	if(encabezado->protocolo != 11)
		/*error*/;
	free(encabezado);

	enum tipoDeInstruccion *tipo;
	recibirMensaje(socketCoord, sizeof(enum tipoDeInstruccion), (void**) &tipo);
	instruccion -> tipo = *tipo;
	free(tipo);
	if(instruccion -> tipo != compactacion)
	{
		tamClave_t *tamClave;
		recibirMensaje(socketCoord, sizeof(tamClave_t), (void**) &tamClave);

		recibirMensaje(socketCoord, *tamClave, (void**) &(instruccion -> clave));
		free(tamClave);
		if(instruccion -> tipo == set)
		{
			tamValor_t *tamValor;
			recibirMensaje(socketCoord, sizeof(tamValor_t), (void**) &tamValor);
			instruccion -> tamValor = *tamValor;

			recibirMensaje(socketCoord, *tamValor, (void**) &(instruccion -> valor));
			free(tamValor);
		}
	}
	return instruccion;
}

void instruccionSet(instruccion_t* instruccion)
{
	if(dictionary_has_key(infoClaves, instruccion -> clave))
	{
		actualizarValorDeClave(instruccion -> clave, instruccion -> valor, instruccion -> tamValor);
	}
	else
	{
		registrarNuevaClave(instruccion -> clave, instruccion -> valor, instruccion -> tamValor);
	}
}

void instruccionStore(instruccion_t* instruccion)
{
	if(dictionary_has_key(infoClaves, instruccion -> clave))
	{
		infoClave *clave = dictionary_get(infoClaves, instruccion -> clave);
		tamClave_t tamClavePendiente = clave -> tamanio;
		int i;
		for(i = 0; tamClavePendiente > 0; i++)
		{
			memcpy(clave -> mappeado,
					tablaDeEntradas[clave -> entradaInicial + i * tamanioEntradas],
					min(tamanioEntradas, tamClavePendiente));
			tablaDeControl[clave -> entradaInicial + i].tiempoUltimoUso = 0;
			tamClavePendiente -= tamanioEntradas;
		}
		/*
		 * Setear valor de respuesta exitoso
		 */
	}
	else
	{
		/*
		 * Informar de Error de Clave no Identificada al coordinador
		 */
	}
}

void actualizarValorDeClave(char* clave, char* valor, tamValor_t tamValor)
{
	infoClave *informacionClave = dictionary_get(infoClaves, clave);
	cantEntradas_t entradasNecesarias = tamValorACantEntradas(tamValor);
	cantEntradas_t entradasUsadas = tamValorACantEntradas(informacionClave -> tamanio);
	if(entradasNecesarias > entradasUsadas)
	{
		actualizarValorMayorTamanio(clave, informacionClave, valor, tamValor);
	}
	else
	{
		actualizarValorMenorTamanio(clave, informacionClave, valor, tamValor);
	}
}

void registrarNuevaClave(char* clave, char* valor, tamValor_t tamValor)
{
	infoClave* nuevaClave = malloc(sizeof(infoClave));

	char* dirArchivo = obtenerDireccionArchivoMontaje(clave);
	nuevaClave -> archivo = fopen(dirArchivo, "w");
	free(dirArchivo);

	cantEntradas_t posicion = encontrarEspacioLibreConsecutivo(tamValor);

	nuevaClave -> entradaInicial = posicion;
	nuevaClave -> tamanio = tamValor;
	crearMappeado(nuevaClave);

	tamValor_t tamValorRestante = tamValor;
	int i;
	for(i = 0; tamValorRestante > 0; i++)
	{
		tablaDeControl[posicion + i].clave = string_duplicate(clave);
		tablaDeControl[posicion + i].tiempoUltimoUso = 0;
		memcpy(tablaDeEntradas[posicion + i * tamanioEntradas],
				valor + i * tamanioEntradas,
				min(tamValorRestante, tamanioEntradas));
		tamValorRestante -= tamanioEntradas;
	}
	dictionary_put(infoClaves, clave, nuevaClave);
}

/*
 * Si quiero actualizar el valor de una clave por otro que requiere más entradas
 * elimino la información de esa Clave y le busco un lugar como si fuera una clave
 * nueva.
 */
void actualizarValorMayorTamanio(char* clave, infoClave* informacionClave, char* valor, tamValor_t tamValor)
{
	int i;
	cantEntradas_t cantEntradasClave = tamValorACantEntradas(informacionClave -> tamanio);
	for(i = 0; i < cantEntradasClave; i++)
	{
		tablaDeControl[informacionClave -> entradaInicial + i].clave=NULL;
	}
	destruirMappeado(informacionClave);
	fclose(informacionClave -> archivo); //¿Hace falta serar el archivo o gasto tiempo innecesariamente?
	free(informacionClave);

	registrarNuevaClave(clave, valor, tamValor);
}

void actualizarValorMenorTamanio(char* clave, infoClave* informacionClave, char* valor, tamValor_t tamValor)
{
	tamValor_t tamValorRestante = tamValor;
	cantEntradas_t entradasRestantes = tamValorACantEntradas(tamValor);
	int i;
	//Coloco el nuevo valor en la tabla de entradas;
	for(i = 0; entradasRestantes > i; i++)
	{
		memcpy(tablaDeEntradas[informacionClave -> entradaInicial + i * tamanioEntradas],
				valor + i * tamanioEntradas,
				min(tamValorRestante, tamanioEntradas));
		tablaDeControl[informacionClave -> entradaInicial + i].tiempoUltimoUso = 0;
		tamValorRestante -= tamanioEntradas;
	}

	/*
	 * Libero las entradas sobrantes.
	 * En caso de que requiera la misma cantidad de entradas no va a entrar al for
	 * haciendo que la función sirva para ese caso tambien.
	 */
	cantEntradas_t finNuevaClave = informacionClave -> entradaInicial + tamValorACantEntradas(tamValor);
	entradasRestantes = tamValorACantEntradas(informacionClave -> tamanio) - tamValorACantEntradas(tamValor);
	for(i = 0; entradasRestantes > i; i++)
		tablaDeControl[finNuevaClave + i].clave = NULL;

	destruirMappeado(informacionClave);
	informacionClave -> tamanio = tamValor;
	crearMappeado(informacionClave);
}

cantEntradas_t encontrarEspacioLibreConsecutivo(tamValor_t tamValor)
{
	cantEntradas_t entradasNecesarias = tamValorACantEntradas(tamValor);
	cantEntradas_t libresConsecutivos = 0;
	int i;
	for(i = 0; i < cantidadEntradas && libresConsecutivos < entradasNecesarias; i++)
	{
		if(tablaDeControl[i].clave)
			libresConsecutivos = 0;
		else
			libresConsecutivos++;
	}
	return i==cantidadEntradas? cantidadEntradas : i - libresConsecutivos;
}

int haySuficienteEspacio(tamValor_t espacioRequerido)
{
	cantEntradas_t entradasRequeridas = tamValorACantEntradas(espacioRequerido);
	cantEntradas_t entradasDisponibles = 0;
	int i;
	for(i = 0; i < cantidadEntradas; i++)
	{
		if(!tablaDeControl[i].clave)
			entradasDisponibles++;
	}
	return entradasRequeridas <= entradasDisponibles;
}

void algoritmoDeReemplazo()
{
	switch(obtenerAlgoritmoReemplazo())
	{
	case Circular:
		reemplazoCircular();
		break;
	case LRU:
	case BSU:
		break;
	}
}

void reemplazoCircular()
{
	infoClave* claveAReemplazar;
	do
	{
		if(dictionary_has_key(infoClaves, tablaDeControl[punteroReemplazo].clave))
			claveAReemplazar = dictionary_get(infoClaves, tablaDeControl[punteroReemplazo].clave);
		else
		{
			incrementarPunteroReemplazo();
			break;
		}

		if(claveAReemplazar -> tamanio < tamanioEntradas)
			break;
		incrementarPunteroReemplazo();
	}while(1); //Asumo que va a existir al menos una clave con valor atomica para reemplazar.

	destruirClave(tablaDeControl[punteroReemplazo].clave);
	tablaDeControl[punteroReemplazo].clave = NULL;
}

void incrementarUltimoUsoEntradas()
{
	int i;
	for(i = 0; i < cantidadEntradas; i++)
		tablaDeControl[i].tiempoUltimoUso++;
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

cantEntradas_t tamValorACantEntradas(tamValor_t v)
{
	return (cantEntradas_t) v / tamanioEntradas + v % tamanioEntradas? 1 : 0;
}

void destruirClave(char* nombreClave)
{
	infoClave* claveADestruir = dictionary_get(infoClaves, nombreClave);
	dictionary_remove(infoClaves, nombreClave);
	destruirMappeado(claveADestruir);
	fclose(claveADestruir->archivo);
	char* dirArchivo = string_from_format("%s%s", obtenerPuntoDeMontaje(), nombreClave);
	remove(dirArchivo);
	free(dirArchivo);
	free(claveADestruir);
}

int crearMappeado(infoClave* clave)
{
	clave -> mappeado = mmap(NULL, clave -> tamanio, PROT_WRITE, MAP_PRIVATE, (int) (clave -> archivo), 0);
	return clave -> mappeado == ERROR? ERROR : 0;
}

int destruirMappeado(infoClave* clave)
{
	int resultado = munmap(clave -> mappeado, clave -> tamanio);
	clave -> mappeado = NULL;
	return resultado;
}

void incrementarPunteroReemplazo()
{
	punteroReemplazo++;
	if(punteroReemplazo == cantidadEntradas)
		punteroReemplazo = 0;
}

int min (int primerValor, int segundoValor)
{
	return primerValor < segundoValor? primerValor : segundoValor;
}

void salirConError(char* error)
{

}
