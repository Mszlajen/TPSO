#include "instancia.h"

char* tablaDeEntradas = NULL;
t_dictionary* infoClaves = NULL;
infoEntrada* tablaDeControl = NULL;
instancia_id ID = ERROR;
tamEntradas_t tamanioEntradas;
cantEntradas_t cantidadEntradas, punteroReemplazo = 0;
socket_t socketCoord = ERROR;

int terminoEjecucion = 0;

int main(int argc, char** argv) {
	inicializar(argv[1]);

	pthread_t hiloProcesamiento;
	pthread_create(&hiloProcesamiento, NULL, (void*) procesamientoInstrucciones, NULL);
	/*pthread_t hiloDump;
	pthread_create(&hiloDump, NULL, (void*) dump, NULL);*/

	pthread_join(hiloProcesamiento, NULL);
	//pthread_join(hiloDump, NULL);

	liberarRecursosGlobales();
	exit(0);
}

void inicializar(char* dirConfig)
{
	if(crearConfiguracion(dirConfig))
		salirConError("No se pudo abrir la configuracion.");
	infoClaves = dictionary_create();
	conectarConCoordinador();
	recibirRespuestaHandshake();
	tablaDeControl = malloc(sizeof(infoEntrada) * cantidadEntradas);
	int i;
	for(i = 0; i < cantidadEntradas; i++)
		tablaDeControl[i].clave = NULL;
}

void dump()
{

}

void procesamientoInstrucciones()
{
	instruccion_t* instruc;
	enum resultadoEjecucion res;
	while(!terminoEjecucion)
	{
		instruc = recibirInstruccionCoordinador();
		switch(instruc -> tipo)
		{
		case set:
			if(obtenerAlgoritmoReemplazo() == LRU)
				incrementarUltimoUsoClaves();
			res = instruccionSet(instruc);
			break;
		case store:
			if(obtenerAlgoritmoReemplazo() == LRU)
				incrementarUltimoUsoClaves();
			res = instruccionStore(instruc);
			break;
		case compactacion:
			break;
		default:
			/*
			 * No me gustan los warnings al pepe.
			 */
			break;
		}
		enviarResultadoEjecucion(res);
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
					 sizeof(cantEntradas_t) +
					 sizeof(cantClaves_t);
	recibirMensaje(socketCoord, tamMensaje, &buffer);
	header head;
	memcpy(&head, buffer, sizeof(header));
	if(ID == ERROR)
	{
		memcpy(&ID, buffer + sizeof(header), sizeof(instancia_id));
	}
	memcpy(&tamanioEntradas, buffer + sizeof(header) + sizeof(instancia_id), sizeof(tamEntradas_t));
	memcpy(&cantidadEntradas, buffer + sizeof(header) + sizeof(instancia_id) + sizeof(tamEntradas_t), sizeof(cantEntradas_t));

	cantClaves_t cantClaves;
	memcpy(&cantClaves, buffer + tamMensaje - sizeof(cantClaves_t), sizeof(cantClaves_t));
	free(buffer);

	tamClave_t* tamClave;
	char *clave, *valor;
	tamValor_t tamValor;
	for(; cantClaves > 0; cantClaves--)
	{
		recibirMensaje(socketCoord, sizeof(tamClave_t), (void**) &tamClave);
		recibirMensaje(socketCoord, *tamClave, (void**) &clave);
		tamValor = leerDeArchivo(clave, &valor);
		if(tamValor)
			registrarNuevaClave(clave, valor, tamValor);
		else
			error_show("No se pudo recuperar la clave %s", clave);
		free(tamClave);
		free(clave);
		free(valor);
	}
	if(head.protocolo != 5)
	{ /* ERROR */ }
}

instruccion_t* recibirInstruccionCoordinador()
{
	listen(socketCoord, 5);
	instruccion_t *instruccion = malloc(sizeof(instruccion_t));

	header* encabezado;
	recibirMensaje(socketCoord, sizeof(header), (void**) &encabezado);
	if(encabezado->protocolo != 11)
	{	/*error*/ }
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

enum resultadoEjecucion instruccionSet(instruccion_t* instruccion)
{
	if(dictionary_has_key(infoClaves, instruccion -> clave))
	{
		return actualizarValorDeClave(instruccion -> clave, instruccion -> valor, instruccion -> tamValor);
	}
	else
	{
		return registrarNuevaClave(instruccion -> clave, instruccion -> valor, instruccion -> tamValor);
	}
}

enum resultadoEjecucion instruccionStore(instruccion_t* instruccion)
{
	if(dictionary_has_key(infoClaves, instruccion -> clave))
	{
		infoClave *clave = dictionary_get(infoClaves, instruccion -> clave);
		clave -> tiempoUltimoUso = 0;
		if(guardarEnArchivo(clave))
			return exito;
	}
	return fallo;
}

enum resultadoEjecucion actualizarValorDeClave(char* clave, char* valor, tamValor_t tamValor)
{
	infoClave *informacionClave = dictionary_get(infoClaves, clave);
	cantEntradas_t entradasNecesarias = tamValorACantEntradas(tamValor);
	cantEntradas_t entradasUsadas = tamValorACantEntradas(informacionClave -> tamanio);
	if(entradasNecesarias > entradasUsadas)
	{
		return actualizarValorMayorTamanio(clave, informacionClave, valor, tamValor);
	}
	else
	{
		return actualizarValorMenorTamanio(clave, informacionClave, valor, tamValor);
	}
}

enum resultadoEjecucion registrarNuevaClave(char* clave, char* valor, tamValor_t tamValor)
{
	infoClave* nuevaClave = malloc(sizeof(infoClave));

	char* dirArchivo = obtenerDireccionArchivoMontaje(clave);
	nuevaClave -> fd = open(dirArchivo, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
	free(dirArchivo);

	ftruncate(nuevaClave -> fd, nuevaClave -> tamanio);

	cantEntradas_t posicion = encontrarEspacioLibreConsecutivo(tamValor);
	if(posicion == cantidadEntradas)
	{
		do
		{
			if(haySuficienteEspacio(tamValor))
			{
				//Compactacion
				posicion = encontrarEspacioLibreConsecutivo(tamValor);
				if(posicion == cantidadEntradas)
					return compactacion;
				else
					break;
			}
			else
				algoritmoDeReemplazo();
		}while(1);
		/*
		 * Reemplaza claves hasta tener espacio suficiente para guardar la clave.
		 */
	}

	nuevaClave -> entradaInicial = posicion;
	nuevaClave -> tamanio = tamValor;
	crearMappeado(nuevaClave);

	nuevaClave ->tiempoUltimoUso = 0;

	memcpy(tablaDeEntradas + posicion * tamanioEntradas, valor, tamValor);

	tamValor_t tamValorRestante = tamValor;
	int i;
	for(i = 0; tamValorRestante > 0; i++)
	{
		tablaDeControl[posicion + i].clave = string_duplicate(clave);
		tamValorRestante -= tamanioEntradas;
	}
	dictionary_put(infoClaves, clave, nuevaClave);
	return exito;
}

enum resultadoEjecucion actualizarValorMayorTamanio(char* clave, infoClave* informacionClave, char* valor, tamValor_t tamValor)
{
	/*
	 * Si quiero actualizar el valor de una clave por otro que requiere más entradas
	 * elimino la información de esa Clave y le busco un lugar como si fuera una clave
	 * nueva.
	 */
	cantEntradas_t cantEntradasClave = tamValorACantEntradas(informacionClave -> tamanio);
	desasociarEntradas(informacionClave -> entradaInicial, cantEntradasClave);
	destruirMappeado(informacionClave);
	close(informacionClave -> fd); //¿Hace falta cerrar el archivo o gasto tiempo innecesariamente?
	free(informacionClave);

	return registrarNuevaClave(clave, valor, tamValor);
}

enum resultadoEjecucion actualizarValorMenorTamanio(char* clave, infoClave* informacionClave, char* valor, tamValor_t tamValor)
{
	cantEntradas_t entradasRestantes = tamValorACantEntradas(tamValor);
	//Coloco el nuevo valor en la tabla de entradas;
	memcpy(tablaDeEntradas + informacionClave -> entradaInicial * tamanioEntradas, valor, tamValor);

	/*
	 * Libero las entradas sobrantes.
	 * En caso de que requiera la misma cantidad de entradas no va a entrar al for
	 * haciendo que la función sirva para ese caso tambien.
	 */
	cantEntradas_t finNuevaClave = informacionClave -> entradaInicial + tamValorACantEntradas(tamValor);
	entradasRestantes = tamValorACantEntradas(informacionClave -> tamanio) - tamValorACantEntradas(tamValor);
	desasociarEntradas(finNuevaClave, entradasRestantes);

	destruirMappeado(informacionClave);
	informacionClave -> tamanio = tamValor;
	crearMappeado(informacionClave);

	return exito;
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

void desasociarEntradas(cantEntradas_t base, cantEntradas_t cantidad)
{
	int i;
	for(i = 0; cantidad > i; i++)
	{
		free(tablaDeControl[base + i].clave);
		tablaDeControl[base + i].clave = NULL;
	}
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

void enviarResultadoEjecucion(enum resultadoEjecucion res)
{
	void* buffer = malloc(sizeof(header) + sizeof(enum resultadoEjecucion));
	((header*) buffer) -> protocolo = 12;
	*((enum resultadoEjecucion*) buffer + sizeof(header)) = res;
	enviarBuffer(socketCoord, buffer, sizeof(header) + sizeof(enum resultadoEjecucion));
}

void incrementarUltimoUsoClaves()
{
	void incrementar(void *clave)
	{
		((infoClave*) clave) -> tiempoUltimoUso++;
	}
	dictionary_iterator(infoClaves, incrementar);
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
	destruirInfoClave(claveADestruir);
	char* dirArchivo = string_from_format("%s%s", obtenerPuntoDeMontaje(), nombreClave);
	remove(dirArchivo);
	free(dirArchivo);
}

void destruirInfoClave(infoClave* clave)
{
	destruirMappeado(clave);
	close(clave -> fd);
	free(clave);
}

int crearMappeado(infoClave* clave)
{
	clave -> mappeado = mmap(NULL, clave -> tamanio, PROT_WRITE, MAP_PRIVATE, clave -> fd, 0);
	return clave -> mappeado == ERROR? ERROR : 0;
}

int destruirMappeado(infoClave* clave)
{
	int resultado = munmap(clave -> mappeado, clave -> tamanio);
	clave -> mappeado = NULL;
	return resultado;
}

int guardarEnArchivo(infoClave* clave)
{
	strcpy(clave -> mappeado, tablaDeEntradas + clave -> entradaInicial * tamanioEntradas);
	return msync(clave -> mappeado, clave -> tamanio, MS_SYNC);
}

tamValor_t leerDeArchivo(char* clave, char** valor)
{
	char aux;
	char *dirArchivo = string_from_format("%s%s", obtenerPuntoDeMontaje(), clave);
	FILE* archivo = fopen(dirArchivo, "r");
	free(dirArchivo);

	if(!archivo)
		return 0;
	tamValor_t tamValor;
	*valor = string_new();
	while(!feof(archivo))
	{
		fread(&aux, sizeof(char), 1, archivo);
		string_append_with_format(valor, "%c", aux);
		tamValor += 1;
	}
	return tamValor;
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

void liberarRecursosGlobales()
{
	if(tablaDeControl)
		free(tablaDeControl);
	if(tablaDeEntradas)
		free(tablaDeEntradas);
	if(infoClaves)
		dictionary_destroy_and_destroy_elements(infoClaves, destruirInfoClave);
	if(socketCoord)
		close(socketCoord);
}

void salirConError(char* error)
{
	error_show(error);
	terminoEjecucion = 1;
}
