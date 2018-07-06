#include "instancia.h"

char* tablaDeEntradas = NULL;
t_dictionary* infoClaves = NULL;

//Variables que podrian no ser globales
infoEntrada_t* tablaDeControl = NULL;
tamEntradas_t tamanioEntradas;
cantEntradas_t cantidadEntradas, punteroReemplazo = 0;
//fin variables que podrian no ser globales

pthread_mutex_t mEnEjecucion;

int terminoEjecucion = 0;

int main(int argc, char** argv) {
	socket_t socketCoord = ERROR;

	inicializar(argv[1], &socketCoord);

	/*pthread_t hiloDump;
	pthread_create(&hiloDump, NULL, (void*) dump, NULL);*/

	while(!terminoEjecucion)
	{
		header* encabezado;
		printf("Esperando instrucciones del coordinador.\n");
		if(recibirMensaje(socketCoord, sizeof(header), (void**) &encabezado) == 1)
		{
			salirConError("Se desconecto el coordinador.\n");
		}
		switch(encabezado -> protocolo)
		{
		case 11:
			printf("Procesando una instrucción.\n");
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

	//pthread_join(hiloDump, NULL);

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
	pthread_mutex_init(&mEnEjecucion, NULL);
}

void dump()
{
	while(!terminoEjecucion)
	{
		sleep(obtenerIntervaloDeDump());
		//Sincronizar con hilo principal
		dictionary_iterator(infoClaves, guardarEnArchivo);
		//Sincronizar con hilo principal
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
	booleano esLRU = obtenerAlgoritmoReemplazo() == LRU, cambioEspacio = 1;
	cantEntradas_t entradasLibres;
	switch(instruc -> tipo)
	{
	case set:
		if(esLRU)
			incrementarUltimoUsoClaves();
		res = instruccionSet(instruc);
		entradasLibres = obtenerEntradasDisponibles();
		break;
	case store:
		if(esLRU)
			incrementarUltimoUsoClaves();
		res = instruccionStore(instruc);
		cambioEspacio = 0;
		break;
	case create:
		if(esLRU)
			incrementarUltimoUsoClaves();
		res = registrarNuevaClave(instruc -> clave, instruc -> valor, instruc -> tamValor);
		entradasLibres = obtenerEntradasDisponibles();
		break;
	case compactacion:
		instruccionCompactacion();
		cambioEspacio = 0;
		break;
	case get:
		/*
		 * No me gustan los warnings al pepe.
		 */
		break;
	}
	enviarResultadoEjecucion(socketCoord, res, cambioEspacio, entradasLibres);
	free(instruc -> clave);
	free(instruc -> valor);
	free(instruc);
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
		fread(&ID, sizeof(instancia_id), 1, archivoID);
		fclose(archivoID);
	}
	else
		ID = 0;

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

	if(archivoID)
		fclose(archivoID);

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
	recibirMensaje(socketCoord, sizeof(instancia_id), (void**) &buffID);
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
		if(!almacenarID(*buffID))
			error_show("No se pudo almacenar la ID\n");

		recibirMensaje(socketCoord, sizeof(cantEntradas_t), (void **) &buffCantEntr);
		cantidadEntradas = *buffCantEntr;
		free(buffCantEntr);

		recibirMensaje(socketCoord, sizeof(tamEntradas_t), (void **) &buffTamEntr);
		tamanioEntradas = *buffTamEntr;
		free(buffTamEntr);
		break;
	}
	free(buffID);
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
		return fallo;
}

enum resultadoEjecucion instruccionCompactacion()
{
	cantEntradas_t punteroVacio = 0, punteroEntrada = 0, distancia;
	infoClave_t *claveAMover;
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

		//Actualizo los valores de la tabla de control
		distancia = punteroEntrada - punteroVacio;
		asociarEntradas(punteroVacio, distancia, tablaDeControl[punteroEntrada].clave);
		punteroVacio = claveAMover -> entradaInicial + tamValorACantEntradas(claveAMover->tamanio);
		asociarEntradas(punteroVacio, distancia, NULL);

		//Actualizo la información de la clave
		claveAMover -> entradaInicial = punteroVacio;
	}
	return exito;
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
		/*
		 * Este es un caso que no está definido y, por lo tanto,
		 * no van a probar pero lo dejo porque ya está todo hecho.
		 */
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


	cantEntradas_t posicion = encontrarEspacioLibreConsecutivo(tamValor);
	if(posicion == cantidadEntradas)
	{
		cantEntradas_t entradasDisponibles = obtenerEntradasDisponibles();
		do
		{
			if(entradasDisponibles >= tamValorACantEntradas(tamValor))
			{
				posicion = encontrarEspacioLibreConsecutivo(tamValor);
				if(posicion == cantidadEntradas)
				{
					/*
					 * Acá podria borrar el archivo, pero como igual lo voy a rehacer
					 * seria procesamiento adicional.
					*/
					close(nuevaClave -> fd);
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
		}while(1);
		/*
		 * Reemplaza claves hasta tener espacio suficiente para guardar la clave.
		 */
	}
	//Abro el archivo o lo creo si no existe
	char* dirArchivo = obtenerDireccionArchivoMontaje(clave);
	nuevaClave -> fd = open(dirArchivo, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
	free(dirArchivo);

	nuevaClave -> entradaInicial = posicion;
	nuevaClave -> tamanio = tamValor - 1;
	nuevaClave ->tiempoUltimoUso = 0;

	ftruncate(nuevaClave -> fd, nuevaClave -> tamanio);
	crearMappeado(nuevaClave);

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
	asociarEntradas(informacionClave -> entradaInicial, cantEntradasClave, NULL);
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
	 * En caso de que requiera la misma cantidad de entradas, entradas restantes va a ser 0
	 * y asociarEntradas no va a hacer nada.
	 */
	cantEntradas_t finNuevaClave = informacionClave -> entradaInicial + tamValorACantEntradas(tamValor);
	entradasRestantes = tamValorACantEntradas(informacionClave -> tamanio) - tamValorACantEntradas(tamValor);
	asociarEntradas(finNuevaClave, entradasRestantes, NULL);

	destruirMappeado(informacionClave);
	informacionClave -> tamanio = tamValor;
	ftruncate(informacionClave -> fd, informacionClave -> tamanio);
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

//BSU y LRU repiten (mucho) codigo, ver que se puede hacer.
infoClave_t* reemplazoLRU()
{
	t_list* candidatos = list_create();
	infoClave_t* claveAReemplazar = NULL;
	int tiempoMayor = 0;
	void obtenerClaveAReemplazar(infoClave_t* infoClave)
	{
		if(infoClave -> tamanio < tamanioEntradas && infoClave -> tiempoUltimoUso >= tiempoMayor)
		{
			if(infoClave -> tiempoUltimoUso > tiempoMayor)
			{
				list_clean(candidatos);
				tiempoMayor = infoClave -> tiempoUltimoUso;
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

infoClave_t* reemplazoBSU()
{
	t_list* candidatos = list_create();
	infoClave_t* claveAReemplazar = NULL;
	tamValor_t tamMayor = 0;
	void obtenerClaveAReemplazar(infoClave_t* infoClave)
	{
		if(infoClave -> tamanio < tamanioEntradas && infoClave -> tiempoUltimoUso >= tamMayor)
		{
			if(infoClave -> tiempoUltimoUso > tamMayor)
			{
				list_clean(candidatos);
				tamMayor = infoClave -> tiempoUltimoUso;
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
	void interacionLiberarNombres(void* infoClave)
	{
		free(tablaDeControl[((infoClave_t*) infoClave) -> entradaInicial].clave);
	}

	if(infoClaves)
	{
		dictionary_iterator(infoClaves,interacionLiberarNombres);
		dictionary_destroy_and_destroy_elements(infoClaves, destruirInfoClave);
	}
	if(tablaDeControl)
		free(tablaDeControl);
	if(tablaDeEntradas)
		free(tablaDeEntradas);
	pthread_mutex_destroy(&mEnEjecucion);
}

void salirConError(char* error)
{
	error_show(error);
	terminoEjecucion = 1;
}
