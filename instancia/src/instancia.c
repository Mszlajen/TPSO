#include "instancia.h"

char* tablaDeEntradas = NULL;
t_dictionary* infoClaves = NULL;
infoEntrada_t* tablaDeControl = NULL;
tamEntradas_t tamanioEntradas;
cantEntradas_t cantidadEntradas, punteroReemplazo = 0;

int terminoEjecucion = 0;

int main(int argc, char** argv) {
	socket_t socketCoord = ERROR;

	inicializar(argv[1], &socketCoord);

	procesamientoInstrucciones(socketCoord);
	/*pthread_t hiloDump;
	pthread_create(&hiloDump, NULL, (void*) dump, NULL);*/
	//pthread_join(hiloDump, NULL);

	liberarRecursosGlobales();
	exit(0);
}

void inicializar(char* dirConfig, socket_t *socketCoord)
{
	if(crearConfiguracion(dirConfig))
		salirConError("No se pudo abrir la configuracion.");
	infoClaves = dictionary_create();
	conectarConCoordinador(socketCoord);
	recibirRespuestaHandshake(*socketCoord);
	//Supuestamente al usar calloc, el valor inicial va a ser NULL de una.
	tablaDeControl = calloc(cantidadEntradas, sizeof(infoEntrada_t));
}

void dump()
{

}

void procesamientoInstrucciones(socket_t socketCoord)
{
	instruccion_t* instruc;
	enum resultadoEjecucion res;
	booleano esLRU = obtenerAlgoritmoReemplazo() == LRU;
	while(!terminoEjecucion)
	{
		instruc = recibirInstruccionCoordinador(socketCoord);
		switch(instruc -> tipo)
		{
		case set:
			if(esLRU)
				incrementarUltimoUsoClaves();
			res = instruccionSet(instruc);
			break;
		case store:
			if(esLRU)
				incrementarUltimoUsoClaves();
			res = instruccionStore(instruc);
			break;
		case create:
			if(esLRU)
				incrementarUltimoUsoClaves();
			res = registrarNuevaClave(instruc -> clave, instruc -> valor, instruc -> tamValor);
			break;
		case compactacion:
			break;
		case get:
			/*
			 * No me gustan los warnings al pepe.
			 */
			break;
		}
		enviarResultadoEjecucion(socketCoord, res);
		free(instruc -> clave);
		free(instruc -> valor);
		free(instruc);
	}
}

void conectarConCoordinador(socket_t *socketCoord)
{
	header head;
	FILE* archivoID = NULL;
	char* dirArchivoID = obtenerDireccionArchivoID();
	void* buffer = NULL;
	instancia_id ID = nuevoID;

	*socketCoord = crearSocketCliente(obtenerIPCordinador(), obtenerPuertoCoordinador());
	if(*socketCoord == ERROR)
		salirConError("No se pudo conectar con el coordinador");

	if((archivoID = fopen(dirArchivoID, "r")))
	{
		fread((void*)&ID, sizeof(instancia_id), 1, archivoID);
		fclose(archivoID);
	}
	else
	{
		tamNombreInstancia_t tamNombre = sizeof(char) * string_length(obtenerNombreInstancia()) +
						sizeof(char);
		int tamBuffer = sizeof(head) +
						sizeof(instancia_id) +
						sizeof(tamNombreInstancia_t) +
						tamNombre;

		buffer = malloc(tamBuffer);
		head.protocolo = 2;
		memcpy(buffer, &head, sizeof(head));
		memcpy(buffer + sizeof(instancia_id), &ID, sizeof(instancia_id));
		memcpy(buffer + sizeof(head) + sizeof(instancia_id), &tamNombre, sizeof(tamNombreInstancia_t));
		memcpy(buffer + sizeof(head) + sizeof(instancia_id) + sizeof(tamNombre), obtenerNombreInstancia(), tamNombre);
		enviarBuffer(*socketCoord, buffer, tamBuffer);
	}
	free(buffer);
	free(dirArchivoID);
}

void recibirRespuestaHandshake(socket_t socketCoord)
{
	header *head;
	recibirMensaje(socketCoord, sizeof(header), (void**)&head);

	cantEntradas_t *buffCantEntr;
	tamEntradas_t *buffTamEntr;
	instancia_id *buffID;
	switch(head -> protocolo)
	{
	case 4:
		recibirMensaje(socketCoord, sizeof(cantEntradas_t), (void **) &buffCantEntr);
		cantidadEntradas = *buffCantEntr;
		free(buffCantEntr);
		recibirMensaje(socketCoord, sizeof(tamEntradas_t), (void **) &buffTamEntr);
		tamanioEntradas = *buffTamEntr;
		free(buffTamEntr);

		cantClaves_t *cantClaves;
		recibirMensaje(socketCoord, sizeof(cantClaves_t), (void **) &cantClaves);

		tamClave_t* tamClave;
		char *clave, *valor;
		tamValor_t tamValor;
		for(; *cantClaves > 0; (*cantClaves)--)
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
		free(cantClaves);
		break;
	case 5:
		recibirMensaje(socketCoord, sizeof(instancia_id), (void**) &buffID);
		if(!almacenarID(*buffID))
			error_show("No se pudo almacenar la ID");
		free(buffID);

		recibirMensaje(socketCoord, sizeof(cantEntradas_t), (void **) &buffCantEntr);
		cantidadEntradas = *buffCantEntr;
		free(buffCantEntr);

		recibirMensaje(socketCoord, sizeof(tamEntradas_t), (void **) &buffTamEntr);
		tamanioEntradas = *buffTamEntr;
		free(buffTamEntr);
		break;
	}
}

instruccion_t* recibirInstruccionCoordinador(socket_t socketCoord)
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
		if(instruccion -> tipo == set || instruccion -> tipo == create)
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
		return actualizarValorDeClave(instruccion -> clave, instruccion -> valor, instruccion -> tamValor);
	else
		return fallo;
}

enum resultadoEjecucion instruccionStore(instruccion_t* instruccion)
{
	if(dictionary_has_key(infoClaves, instruccion -> clave))
	{
		infoClave_t *clave = dictionary_get(infoClaves, instruccion -> clave);
		clave -> tiempoUltimoUso = 0;
		if(guardarEnArchivo(clave))
			return exito;
	}
	return fallo;
}

enum resultadoEjecucion actualizarValorDeClave(char* clave, char* valor, tamValor_t tamValor)
{
	infoClave_t *informacionClave = dictionary_get(infoClaves, clave);
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
	infoClave_t* nuevaClave = malloc(sizeof(infoClave_t));

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
					return necesitaCompactar;
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
	char * claveAsociada = string_duplicate(clave);
	int i;
	for(i = 0; tamValorRestante > 0; i++)
	{
		tablaDeControl[posicion + i].clave = claveAsociada;
		tamValorRestante -= tamanioEntradas;
	}
	dictionary_put(infoClaves, clave, nuevaClave);
	return exito;
}

enum resultadoEjecucion actualizarValorMayorTamanio(char* clave, infoClave_t* informacionClave, char* valor, tamValor_t tamValor)
{
	/*
	 * Si quiero actualizar el valor de una clave por otro que requiere más entradas
	 * elimino la información de esa Clave y le busco un lugar como si fuera una clave
	 * nueva.
	 * Habria que ver si tiene espacio contiguo para creqcer inmediatamente sin hacer
	 * todo el movimiento.
	 */
	cantEntradas_t cantEntradasClave = tamValorACantEntradas(informacionClave -> tamanio);
	desasociarEntradas(informacionClave -> entradaInicial, cantEntradasClave);
	destruirMappeado(informacionClave);
	close(informacionClave -> fd); //¿Hace falta cerrar el archivo o gasto tiempo innecesariamente?
	free(informacionClave);

	return registrarNuevaClave(clave, valor, tamValor);
}

enum resultadoEjecucion actualizarValorMenorTamanio(char* clave, infoClave_t* informacionClave, char* valor, tamValor_t tamValor)
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
	free(tablaDeControl[base].clave);
	for(i = 0; cantidad > i; i++)
		tablaDeControl[base + i].clave = NULL;
}

void algoritmoDeReemplazo()
{
	infoClave_t* claveAReemplazar = NULL;
	switch(obtenerAlgoritmoReemplazo())
	{
	case Circular:
		claveAReemplazar = reemplazoCircular();
		break;
	case LRU:
		claveAReemplazar = reemplazoLRU();
		break;
	case BSU:
		claveAReemplazar = reemplazoBSU();
		break;
	}
	free(tablaDeControl[claveAReemplazar->entradaInicial].clave);
	tablaDeControl[claveAReemplazar->entradaInicial].clave = NULL;
	destruirClave(tablaDeControl[claveAReemplazar->entradaInicial].clave);
}

/*
 * En todos los algoritmos asumo que existe al menos una clave reemplazable.
 */

infoClave_t* reemplazoCircular()
{
	do
	{
		if(tablaDeControl[punteroReemplazo].clave &&
			((infoClave_t*) dictionary_get(infoClaves, tablaDeControl[punteroReemplazo].clave)) -> tamanio < tamanioEntradas)
		{
			incrementarPunteroReemplazo();
			continue;
		}
		incrementarPunteroReemplazo();
		break;
	}while(1);
	return (infoClave_t*) dictionary_get(infoClaves, tablaDeControl[punteroReemplazo].clave);
}

infoClave_t* reemplazoLRU()
{
	char* nombreClaveAReemplazar = NULL;
	int tiempoMayor = 0;
	void obtenerClaveAReemplazar(infoClave_t* infoClave)
	{
		if(infoClave -> tamanio < tamanioEntradas && infoClave -> tiempoUltimoUso > tiempoMayor)
		{
			nombreClaveAReemplazar = tablaDeControl[infoClave -> entradaInicial].clave;
			tiempoMayor = infoClave -> tiempoUltimoUso;
		}
	}

	dictionary_iterator(infoClaves, obtenerClaveAReemplazar);
	return dictionary_get(infoClaves, nombreClaveAReemplazar);
}

infoClave_t* reemplazoBSU()
{
	char* nombreClaveAReemplazar = NULL;
	tamValor_t tamMayor = 0;
	void obtenerClaveAReemplazar(infoClave_t* infoClave)
	{
		if(infoClave -> tamanio < tamanioEntradas && infoClave -> tamanio > tamMayor)
		{
			nombreClaveAReemplazar = tablaDeControl[infoClave -> entradaInicial].clave;
			tamMayor = infoClave -> tiempoUltimoUso;
		}
	}

	dictionary_iterator(infoClaves, obtenerClaveAReemplazar);
	return dictionary_get(infoClaves, nombreClaveAReemplazar);
}

void enviarResultadoEjecucion(socket_t socketCoord, enum resultadoEjecucion res)
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
		((infoClave_t*) clave) -> tiempoUltimoUso++;
	}
	dictionary_iterator(infoClaves, incrementar);
}

booleano almacenarID(instancia_id ID)
{
	FILE* archivo = NULL;
	char* dirArchivo = obtenerDireccionArchivoID();
	booleano resultado = 0;
	if((archivo = fopen(dirArchivo, "w")))
	{
		fwrite((void*)&ID, sizeof(instancia_id), 1, archivo);
		fclose(archivo);
		resultado = 1;
	}
	free(dirArchivo);
	return resultado;
}

cantEntradas_t tamValorACantEntradas(tamValor_t v)
{
	return (cantEntradas_t) v / tamanioEntradas + v % tamanioEntradas? 1 : 0;
}

void destruirClave(char* nombreClave)
{
	infoClave_t* claveADestruir = dictionary_get(infoClaves, nombreClave);
	dictionary_remove(infoClaves, nombreClave);
	destruirInfoClave(claveADestruir);
	char* dirArchivo = string_from_format("%s%s", obtenerPuntoDeMontaje(), nombreClave);
	remove(dirArchivo);
	free(dirArchivo);
}

void destruirInfoClave(infoClave_t* clave)
{
	destruirMappeado(clave);
	close(clave -> fd);
	free(clave);
}

int crearMappeado(infoClave_t* clave)
{
	clave -> mappeado = mmap(NULL, clave -> tamanio, PROT_WRITE, MAP_PRIVATE, clave -> fd, 0);
	return clave -> mappeado == ERROR? ERROR : 0;
}

int destruirMappeado(infoClave_t* clave)
{
	int resultado = munmap(clave -> mappeado, clave -> tamanio);
	clave -> mappeado = NULL;
	return resultado;
}

int guardarEnArchivo(infoClave_t* clave)
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

int max (int primerValor, int segundoValor)
{
	return primerValor > segundoValor? primerValor : segundoValor;
}

void liberarRecursosGlobales()
{
	if(tablaDeControl)
		free(tablaDeControl);
	if(tablaDeEntradas)
		free(tablaDeEntradas);
	if(infoClaves)
		dictionary_destroy_and_destroy_elements(infoClaves, destruirInfoClave);
}

void salirConError(char* error)
{
	error_show(error);
	terminoEjecucion = 1;
}
