#include "instancia.h"

char* tablaDeEntradas = NULL;
t_dictionary* infoClaves = NULL;

//Variables que podrian no ser globales
infoEntrada_t* tablaDeControl = NULL;
tamEntradas_t tamanioEntradas;
cantEntradas_t cantidadEntradas, punteroReemplazo = 0;
instancia_id *id;
//fin variables que podrian no ser globales

pthread_mutex_t mTablaDeEntradas = PTHREAD_MUTEX_INITIALIZER;

int terminoEjecucion = 0;
unsigned long int instruccionActual = 0;

int main(int argc, char** argv)
{
	socket_t socketCoord = ERROR;

	inicializar(argv[1], &socketCoord);

	pthread_t hiloDump;
	pthread_create(&hiloDump, NULL, (void*) dump, NULL);

	while(!terminoEjecucion)
	{
		header* encabezado;
		//printf("Esperando instrucciones del coordinador.\n");
		if(recibirMensaje(socketCoord, sizeof(header), (void**) &encabezado))
		{
			salirConError("Se desconecto el coordinador.\n");
			continue;
		}
		switch(encabezado -> protocolo)
		{
		case 11:
			//printf("Procesando una instrucción.\n");
			procesamientoInstrucciones(socketCoord);
			break;
		case 13:
			//Status
			procesamientoStatus(socketCoord);
			break;
		default:
			//Error
			break;
		}
	}

	pthread_join(hiloDump, NULL);

	liberarRecursosGlobales();
	exit(0);
}

void inicializar(char* dirConfig, socket_t *socketCoord)
{
	if(crearConfiguracion(dirConfig))
		salirConError("No se pudo abrir la configuracion.\n");
	infoClaves = dictionary_create();
	conectarConCoordinador(socketCoord);
	recibirRespuestaHandshake(*socketCoord);
	//Supuestamente al usar calloc, el valor inicial va a ser NULL de una.
	tablaDeControl = calloc(cantidadEntradas, sizeof(infoEntrada_t));
	tablaDeEntradas = malloc(cantidadEntradas * tamanioEntradas);
}

void dump()
{
	void dumpear(char* nombreClave, infoClave_t *clave)
	{
		printf("%s posicion inicial %i\n", nombreClave, clave -> entradaInicial);
		guardarEnArchivo(nombreClave, clave);
	}
	while(!terminoEjecucion)
	{
		sleep((unsigned int) obtenerIntervaloDeDump());
		pthread_mutex_lock(&mTablaDeEntradas);
		dictionary_iterator(infoClaves, dumpear);
		pthread_mutex_unlock(&mTablaDeEntradas);
	}
}

void procesamientoStatus(socket_t socketCoord)
{
	tamClave_t *tamClave = NULL;
	char* clave = NULL;
	recibirMensaje(socketCoord, sizeof(tamClave_t), (void**) &tamClave);
	recibirMensaje(socketCoord, *tamClave, (void**) &clave);
	free(tamClave);

	header head;
	head.protocolo = 15;
	enviarHeader(socketCoord, head);

	booleano existe = dictionary_has_key(infoClaves, clave);
	enviarBuffer(socketCoord, (void*) &existe, sizeof(booleano));

	if(existe)
	{
		//El valor existe

		//Recupero el Valor de la clave
		infoClave_t *infoClave = dictionary_get(infoClaves, clave);
		tamValor_t tamValor = infoClave -> tamanio + 1;
		char* valor = malloc(tamValor);
		char nulo = '\0';
		memcpy(valor, tablaDeEntradas + infoClave -> entradaInicial * tamanioEntradas, infoClave -> tamanio);
		memcpy(valor + tamValor - 1, (void*) &nulo, sizeof(char));

		enviarBuffer(socketCoord, (void*) &tamValor, sizeof(tamValor));
		enviarBuffer(socketCoord, (void*) valor, tamValor);

		free(valor);
	}
}

void procesamientoInstrucciones(socket_t socketCoord)
{
	instruccion_t* instruc = recibirInstruccionCoordinador(socketCoord);
	enum resultadoEjecucion res;
	booleano cambioEspacio = 1;
	cantEntradas_t entradasLibres;
	switch(instruc -> tipo)
	{
	case set:
		pthread_mutex_lock(&mTablaDeEntradas);
		res = instruccionSet(instruc);
		pthread_mutex_unlock(&mTablaDeEntradas);
		entradasLibres = obtenerEntradasDisponibles();
		free(instruc -> clave);
		free(instruc -> valor);
		instruccionActual++;
		break;
	case store:
		res = instruccionStore(instruc);
		cambioEspacio = 0;
		free(instruc -> clave);
		instruccionActual++;
		break;
	case create:
		pthread_mutex_lock(&mTablaDeEntradas);
		res = registrarNuevaClave(instruc -> clave, instruc -> valor, instruc -> tamValor);
		pthread_mutex_unlock(&mTablaDeEntradas);
		entradasLibres = obtenerEntradasDisponibles();
		free(instruc -> clave);
		free(instruc -> valor);
		instruccionActual++;
		break;
	case compactacion:
		pthread_mutex_lock(&mTablaDeEntradas);
		instruccionCompactacion();
		pthread_mutex_unlock(&mTablaDeEntradas);
		cambioEspacio = 0;
		break;
	case get:
		/*
		 * No me gustan los warnings al pepe.
		 */
		break;
	}
	enviarResultadoEjecucion(socketCoord, res, cambioEspacio, entradasLibres);
	free(instruc);
}

void conectarConCoordinador(socket_t *socketCoord)
{
	header head;
	FILE* archivoID = NULL;
	char* dirArchivoID = obtenerDireccionArchivoID();
	void* buffer = NULL;

	id = malloc(sizeof(instancia_id));

	*socketCoord = crearSocketCliente(obtenerIPCordinador(), obtenerPuertoCoordinador());
	if(*socketCoord == ERROR)
		salirConError("No se pudo conectar con el coordinador");

	if((archivoID = fopen(dirArchivoID, "r")))
	{
		fread(id, sizeof(instancia_id), 1, archivoID);
		fclose(archivoID);
	}
	else
		*id = 0;

	tamNombreInstancia_t tamNombre = sizeof(char) * string_length(obtenerNombreInstancia()) +
					sizeof(char);
	int tamBuffer = sizeof(head) +
					sizeof(instancia_id) +
					sizeof(tamNombreInstancia_t) +
					tamNombre;

	buffer = malloc(tamBuffer);
	head.protocolo = 2;
	memcpy(buffer, &head, sizeof(head));
	memcpy(buffer + sizeof(instancia_id), id, sizeof(instancia_id));
	memcpy(buffer + sizeof(head) + sizeof(instancia_id), &tamNombre, sizeof(tamNombreInstancia_t));
	memcpy(buffer + sizeof(head) + sizeof(instancia_id) + sizeof(tamNombre), obtenerNombreInstancia(), tamNombre);
	enviarBuffer(*socketCoord, buffer, tamBuffer);

	free(buffer);
	free(dirArchivoID);
	free(id);
}

void recibirRespuestaHandshake(socket_t socketCoord)
{
	header *head;
	recibirMensaje(socketCoord, sizeof(header), (void**)&head);

	cantEntradas_t *buffCantEntr;
	tamEntradas_t *buffTamEntr;
	recibirMensaje(socketCoord, sizeof(instancia_id), (void**) &id);
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
				error_show("No se pudo recuperar la clave %s\n", clave);
			free(tamClave);
			free(clave);
			free(valor);
		}
		*cantClaves = obtenerEntradasDisponibles();
		enviarBuffer(socketCoord, (void*) head, sizeof(header));
		enviarBuffer(socketCoord, (void*) cantClaves, sizeof(cantClaves_t));
		free(cantClaves);

		break;
	case 5:
		if(!almacenarID(*id))
			error_show("No se pudo almacenar la ID\n");

		recibirMensaje(socketCoord, sizeof(cantEntradas_t), (void **) &buffCantEntr);
		cantidadEntradas = *buffCantEntr;
		free(buffCantEntr);

		recibirMensaje(socketCoord, sizeof(tamEntradas_t), (void **) &buffTamEntr);
		tamanioEntradas = *buffTamEntr;
		free(buffTamEntr);
		break;
	}
	free(head);
}

instruccion_t* recibirInstruccionCoordinador(socket_t socketCoord)
{
	instruccion_t *instruccion = malloc(sizeof(instruccion_t));

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
		return fNoIdentificada;
}

enum resultadoEjecucion instruccionCompactacion()
{
	cantEntradas_t punteroVacio = 0, punteroEntrada = 0, distancia;
	infoClave_t *claveAMover;
	printf("Se está realizando la compactación.\n");
	while(1)
	{
		//Busco el proximo espacio vacio
		//Desde la segunda iteracion puntero vacio va a empezar en el valor correspondiente
		for(;punteroVacio < cantidadEntradas && tablaDeControl[punteroVacio].clave; punteroVacio++);
		//Compruebo si salio por encontrar un espacio vacio o porque termino de compactar
		if(punteroVacio == cantidadEntradas)
			break;

		//Busco el proximo espacio ocupado empezando por el siguiente al vacio que ya encontre
		punteroEntrada = punteroVacio + 1;
		for(;punteroEntrada < cantidadEntradas && !tablaDeControl[punteroEntrada].clave; punteroEntrada++);
		//Compruebo si salio por encontrar un espacio ocupado o porque termino de compactar
		if(punteroEntrada == cantidadEntradas)
			break;

		//Recupero la información de la clave ocupando el espacio
		claveAMover = dictionary_get(infoClaves, tablaDeControl[punteroEntrada].clave);

		//Muevo la informacion de manera que empiece en el lugar vacio
		memcpy(tablaDeEntradas + punteroVacio * tamanioEntradas,
				tablaDeEntradas + punteroEntrada * tamanioEntradas,
				claveAMover -> tamanio);
		
		//Actualizo la información de la clave
		claveAMover -> entradaInicial = punteroVacio;

		//Actualizo los valores de la tabla de control
		distancia = punteroEntrada - punteroVacio;
		asociarEntradas(punteroVacio, distancia, tablaDeControl[punteroEntrada].clave);
		punteroVacio = punteroVacio + tamValorACantEntradas(claveAMover->tamanio);
		asociarEntradas(punteroVacio, distancia, NULL);

	}
	return exito;
}

enum resultadoEjecucion instruccionStore(instruccion_t* instruccion)
{
	if(dictionary_has_key(infoClaves, instruccion -> clave))
	{
		infoClave_t *clave = dictionary_get(infoClaves, instruccion -> clave);
		clave -> tiempoUltimoUso = instruccionActual;
		if(guardarEnArchivo(instruccion -> clave, clave))
			return exito;
		else
			return fNoAlmaceno;
	}
	return fNoIdentificada;
}

enum resultadoEjecucion actualizarValorDeClave(char* clave, char* valor, tamValor_t tamValor)
{
	infoClave_t *informacionClave = dictionary_get(infoClaves, clave);
	cantEntradas_t entradasNecesarias = tamValorACantEntradas(tamValor);
	cantEntradas_t entradasUsadas = tamValorACantEntradas(informacionClave -> tamanio);
	if(entradasNecesarias > entradasUsadas)
	{
		return fIncrementaValor;
	}
	else
	{
		informacionClave -> tiempoUltimoUso = instruccionActual;
		return actualizarValorMenorTamanio(clave, informacionClave, valor, tamValor);
	}
}

enum resultadoEjecucion registrarNuevaClave(char* clave, char* valor, tamValor_t tamValor)
{
	infoClave_t* nuevaClave = malloc(sizeof(infoClave_t));
	nuevaClave -> tamanio = tamValor - 1;

	cantEntradas_t posicion = encontrarEspacioLibreConsecutivo(nuevaClave -> tamanio);
	if(posicion == cantidadEntradas)
	{
		cantEntradas_t entradasDisponibles = obtenerEntradasDisponibles();
		do
		{
			if(entradasDisponibles >= tamValorACantEntradas(nuevaClave -> tamanio))
			{
				posicion = encontrarEspacioLibreConsecutivo(nuevaClave -> tamanio);
				if(posicion == cantidadEntradas)
				{
					free(nuevaClave);
					return necesitaCompactar;
				}
				else
					break;
			}
			else
			{
				algoritmoDeReemplazo();
				entradasDisponibles++;
			}
		}while(1); // Reemplaza claves hasta tener espacio suficiente para guardar la clave.
	}
	//Abro el archivo o lo creo si no existe
	char* dirArchivo = obtenerDireccionArchivoMontaje(clave);
	nuevaClave -> archivo = fopen(dirArchivo, "w");
	free(dirArchivo);

	nuevaClave -> entradaInicial = posicion;
	nuevaClave -> tiempoUltimoUso = instruccionActual;
	pthread_mutex_init(&(nuevaClave -> mArchivo), NULL);

	//Coloco el valor en la tabla de entradas.
	memcpy(tablaDeEntradas + posicion * tamanioEntradas, valor, nuevaClave -> tamanio);

	//Actualizo la tabla de control
	char * claveAsociada = string_duplicate(clave);
	asociarEntradas(nuevaClave -> entradaInicial, tamValorACantEntradas(nuevaClave -> tamanio), claveAsociada);

	//Guardo la información de la clave.
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
	free(tablaDeControl[informacionClave->entradaInicial].clave);
	asociarEntradas(informacionClave -> entradaInicial, cantEntradasClave, NULL);;
	fclose(informacionClave -> archivo); //¿Hace falta cerrar el archivo o gasto tiempo innecesariamente?
	free(informacionClave);

	return registrarNuevaClave(clave, valor, tamValor);
}

enum resultadoEjecucion actualizarValorMenorTamanio(char* clave, infoClave_t* informacionClave, char* valor, tamValor_t tamValor)
{
	cantEntradas_t entradasRestantes = tamValorACantEntradas(tamValor - 1);
	//Coloco el nuevo valor en la tabla de entradas;
	memcpy(tablaDeEntradas + informacionClave -> entradaInicial * tamanioEntradas, valor, tamValor - 1);

	/*
	 * Libero las entradas sobrantes.
	 * En caso de que requiera la misma cantidad de entradas, entradas restantes va a ser 0
	 * y asociarEntradas no va a hacer nada.
	 */
	cantEntradas_t finNuevaClave = informacionClave -> entradaInicial + entradasRestantes;
	entradasRestantes = tamValorACantEntradas(informacionClave -> tamanio) - entradasRestantes;
	asociarEntradas(finNuevaClave, entradasRestantes, NULL);

	informacionClave -> tamanio = tamValor - 1;
	ftruncate(fileno(informacionClave->archivo), informacionClave -> tamanio);

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
	return libresConsecutivos < entradasNecesarias? cantidadEntradas : i - libresConsecutivos;
}

cantEntradas_t obtenerEntradasDisponibles()
{
	cantEntradas_t entradasDisponibles = 0;
	int i;
	for(i = 0; i < cantidadEntradas; i++)
	{
		if(!tablaDeControl[i].clave)
			entradasDisponibles++;
	}
	return entradasDisponibles;
}

void asociarEntradas(cantEntradas_t base, cantEntradas_t cantidad, char* clave)
{
	int i;
	for(i = 0; cantidad > i; i++)
		tablaDeControl[base + i].clave = clave;
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
	printf("Se reemplazo la clave %s\n", tablaDeControl[claveAReemplazar->entradaInicial].clave);
	int posicionReemplazada = claveAReemplazar->entradaInicial;
	destruirClave(tablaDeControl[posicionReemplazada].clave);
	free(tablaDeControl[posicionReemplazada].clave);
	tablaDeControl[posicionReemplazada].clave = NULL;
}

/*
 * En todos los algoritmos asumo que existe al menos una clave reemplazable.
 */

infoClave_t* reemplazoCircular()
{
	infoClave_t *candidato;
	do
	{
		if(!tablaDeControl[punteroReemplazo].clave)
		{
			incrementarPunteroReemplazo();
			continue;
		}
		candidato = dictionary_get(infoClaves, tablaDeControl[punteroReemplazo].clave);
		if(candidato -> tamanio <= tamanioEntradas)
			break;
		else
			incrementarPunteroReemplazo();
	}while(1);
	incrementarPunteroReemplazo();
	return candidato;
}

//BSU y LRU repiten (mucho) codigo, ver que se puede hacer.
infoClave_t* reemplazoLRU()
{
	infoClave_t* claveAReemplazar = NULL;
	unsigned long int mayorAntiguedad = ULONG_MAX;
	void obtenerClaveAReemplazar(char* clave, infoClave_t* infoClave)
	{
		if(infoClave -> tamanio <= tamanioEntradas && infoClave -> tiempoUltimoUso <= mayorAntiguedad)
		{
			claveAReemplazar = infoClave;
			mayorAntiguedad = infoClave -> tiempoUltimoUso;
		}
	}

	dictionary_iterator(infoClaves, obtenerClaveAReemplazar);
	return claveAReemplazar;
}

infoClave_t* reemplazoBSU()
{
	t_list* candidatos = list_create();
	infoClave_t* claveAReemplazar = NULL;
	tamValor_t tamMayor = 0;
	void obtenerClaveAReemplazar(char* clave, infoClave_t* infoClave)
	{
		if(infoClave -> tamanio <= tamanioEntradas && infoClave -> tamanio >= tamMayor)
		{
			if(infoClave -> tamanio > tamMayor)
			{
				list_clean(candidatos);
				tamMayor = infoClave -> tamanio;
			}
			list_add(candidatos, infoClave);
		}
	}

	dictionary_iterator(infoClaves, obtenerClaveAReemplazar);
	if(list_size(candidatos) == 1)
		claveAReemplazar = list_get(candidatos, 0);
	else
		claveAReemplazar = desempatePorCircular(candidatos);
	list_destroy(candidatos);
	return claveAReemplazar;
}

infoClave_t* desempatePorCircular(t_list* candidatos)
{
	infoClave_t* claveAReemplazar = NULL;
	cantEntradas_t posicionMejorCandidato = cantidadEntradas;
	void esMasCercano(infoClave_t *candidatoActual)
	{
		if(punteroReemplazo <= candidatoActual -> entradaInicial && candidatoActual -> entradaInicial <= posicionMejorCandidato)
		{
			claveAReemplazar = candidatoActual;
			posicionMejorCandidato = candidatoActual -> entradaInicial;
		}
	}
	void menorPosicion(infoClave_t *candidatoActual)
	{
		if(candidatoActual -> entradaInicial < posicionMejorCandidato)
		{
			claveAReemplazar = candidatoActual;
			posicionMejorCandidato = candidatoActual -> entradaInicial;
		}
	}

	list_iterate(candidatos, esMasCercano);
	if(!claveAReemplazar)
		list_iterate(candidatos, menorPosicion);

	punteroReemplazo = claveAReemplazar -> entradaInicial;
	incrementarPunteroReemplazo();
	return claveAReemplazar;
}

void enviarResultadoEjecucion(socket_t socketCoord, enum resultadoEjecucion res, booleano enviarEspacio, cantEntradas_t espacio)
{
	header head;
	head.protocolo = 12;
	enviarHeader(socketCoord, head);
	enviarBuffer(socketCoord, (void*) &res, sizeof(res));
	if(enviarEspacio)
		enviarBuffer(socketCoord, (void*) &espacio, sizeof(espacio));
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
		resultado = fwrite(&ID, sizeof(instancia_id), 1, archivo) == 1;
		fclose(archivo);
	}
	free(dirArchivo);
	return resultado;
}

cantEntradas_t tamValorACantEntradas(tamValor_t v)
{
	return (cantEntradas_t) v / tamanioEntradas + (v % tamanioEntradas? 1 : 0);
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
	fclose(clave -> archivo);
	pthread_mutex_destroy(&(clave -> mArchivo));
	free(clave);
}

int guardarEnArchivo(char* nombreClave, infoClave_t* clave)
{
	pthread_mutex_lock(&(clave -> mArchivo));
	uint8_t escrito =
			fwrite(tablaDeEntradas + clave -> entradaInicial * tamanioEntradas, sizeof(char), clave -> tamanio, clave -> archivo);
	rewind(clave->archivo);
	pthread_mutex_unlock(&(clave -> mArchivo));
	return clave -> tamanio == escrito;
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
	fclose(archivo);
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
	void interacionLiberarNombres(char* clave, void* infoClave)
	{
		free(tablaDeControl[((infoClave_t*) infoClave) -> entradaInicial].clave);
	}

	if(infoClaves)
	{
		dictionary_iterator(infoClaves, interacionLiberarNombres);
		dictionary_destroy_and_destroy_elements(infoClaves, destruirInfoClave);
	}
	if(tablaDeControl)
		free(tablaDeControl);
	if(tablaDeEntradas)
		free(tablaDeEntradas);
	pthread_mutex_destroy(&mTablaDeEntradas);
}

void salirConError(char* error)
{
	error_show(error);
	terminoEjecucion = 1;
}
