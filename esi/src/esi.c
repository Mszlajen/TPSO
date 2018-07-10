#include "esi.h"

int main(int argc, char **argv) {
	t_config * configuracion = NULL;
	FILE * programa = NULL;
	socket_t socketCoord = ERROR, socketPlan = ERROR;
	ESI_id *id;
	header *head;

	inicializacion(argc, argv, &configuracion, &programa);

	socketCoord = conectarConCoordinador(configuracion);
	enviarHandshake(socketCoord);

	socketPlan = conectarConPlanificador(configuracion);
	enviarHandshake(socketPlan);
	recibirMensaje(socketPlan, sizeof(header), (void**) &head);
	recibirMensaje(socketPlan, sizeof(ESI_id), (void**) &id);

	t_esi_operacion operacionESI;
	booleano hayInstruccion = leerSiguienteInstruccion(programa, &operacionESI);
	enum resultadoEjecucion *resultado;
	while (hayInstruccion)
	{
		if(!operacionESI.valido)
		{
			error_show("Abortando por error de tamaño de clave.\n");
			break;
		}
		printf("Esperando aviso de ejecución.\n");
		if(recibirMensaje(socketPlan, sizeof(header), (void**) &head))
		{
			error_show("Se desconecto el planificador.\n");
			break;
		}
		if(head -> protocolo != 7)
		{/* ERROR */}
		free(head);
		printf("Enviando instrucción al coordinador.\n");
		enviarInstruccionCoord(socketCoord, operacionESI, *id);
		destruir_operacion(operacionESI);
		printf("Esperando resultado de ejecución.\n");
		if(recibirMensaje(socketCoord, sizeof(header), (void**) &head))
		{
			error_show("Se desconecto el coordinador.\n");
			break;
		}
		if(head -> protocolo != 12)
		{ /*ERROR*/ }
		recibirMensaje(socketCoord, sizeof(enum resultadoEjecucion), (void**) &resultado);

		hayInstruccion = leerSiguienteInstruccion(programa, &operacionESI);
		printf("Enviando resultado de ejecución al planificador.\n");
		enviarResultadoPlanificador(socketPlan, *resultado, !hayInstruccion);
	}
	printf("Termino la ejecución.\n");
	free(id);
	fclose(programa);
	config_destroy(configuracion);
	close(socketCoord);
	close(socketPlan);
	exit(0);
}

void inicializacion(int argc, char** argv, t_config **configuracion, FILE **programa)
{
	if(argc != 3)
		salirConError("Cantidad de parametros invalida\n");
	*programa = abrirArchivoLectura(argv[2]);
	if(!*programa)
		salirConError("No se pudo abrir las instrucciones del ESI.\n");

	*configuracion = config_create(argv[1]);
	if(!*configuracion)
		salirConError("Fallo al abrir el archivo de configuracion.\n");
}

socket_t conectarConCoordinador(t_config *configuracion)
{
	char * IP = config_get_string_value(configuracion, IPCoord);
	char * puerto = config_get_string_value(configuracion, PuertoCoord);
	socket_t socketCoord = crearSocketCliente(IP, puerto);
	if(socketCoord == ERROR)
		salirConError("No se pudo conectar al coordinador.\n");
	return socketCoord;
}

socket_t conectarConPlanificador(t_config* configuracion)
{
	char * IP = config_get_string_value(configuracion, IPPlan);
	char * puerto = config_get_string_value(configuracion, PuertoPlan);
	socket_t socketPlan = crearSocketCliente(IP, puerto);
	if(socketPlan == ERROR)
		salirConError("No se pudo conectar al coordinador.\n");
	return socketPlan;
}

FILE* abrirArchivoLectura(char* nombreArchivo)
{
	return fopen(nombreArchivo, "r");
}

void enviarHandshake(socket_t socket)
{
	header mensaje;
	mensaje.protocolo = 3;
	if(enviarHeader(socket, mensaje) == ERROR)
		salirConError("No se pudo enviar mensaje handshake.\n");
}

void salirConError(char* error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}

void liberarRecursos()
{
}

booleano leerSiguienteInstruccion(FILE* programa, t_esi_operacion *operacion)
{
	char * linea = string_new();
	char leido;

	while (!feof(programa) && fread(&leido, sizeof(char),1,programa) && leido != '\n'){
		string_append_with_format(&linea, "%c", leido);
	}
	if(string_is_empty(linea))
		return 0;
	*operacion = parse(linea);
	free(linea);
	return 1;
}

void enviarInstruccionCoord(socket_t socketCoord, t_esi_operacion operacion, ESI_id id)
{
	header header;
	header.protocolo = 8;
	enum instruccion instr;
	switch(operacion.keyword)
	{
	case GET:
		instr = get;
		break;
	case SET:
		instr = set;
		break;
	case STORE:
		instr = store;
	}
	tamClave_t tamClave;
	tamValor_t tamValor;

	enviarHeader(socketCoord, header);
	enviarBuffer(socketCoord, &id, sizeof(id));
	enviarBuffer(socketCoord, &instr, sizeof(instr));

	switch(operacion.keyword)
	{
			case GET:
				tamClave = string_length(operacion.argumentos.GET.clave) + 1;
				enviarBuffer(socketCoord, &tamClave, sizeof(tamClave));
				enviarBuffer(socketCoord, operacion.argumentos.GET.clave, tamClave);
				break;
			case SET:
				tamClave = string_length(operacion.argumentos.SET.clave) + 1;
				enviarBuffer(socketCoord, &tamClave, sizeof(tamClave));
				enviarBuffer(socketCoord, operacion.argumentos.SET.clave, tamClave);

				tamValor = string_length(operacion.argumentos.SET.valor) + 1;
				enviarBuffer(socketCoord, &tamValor, sizeof(tamValor));
				enviarBuffer(socketCoord, operacion.argumentos.SET.valor, tamValor);
				break;
			case STORE:
				tamClave = string_length(operacion.argumentos.STORE.clave) + 1;
				enviarBuffer(socketCoord, &tamClave, sizeof(tamClave));
				enviarBuffer(socketCoord, operacion.argumentos.STORE.clave, tamClave);
				break;
	}
}

void enviarResultadoPlanificador(socket_t socketPlan, enum resultadoEjecucion resultado, booleano termino)
{
	header head;
	head.protocolo = 12;
	if(termino && resultado != fallo)
		resultado = fin;
	enviarHeader(socketPlan, head);
	enviarBuffer(socketPlan, (void*) &resultado, sizeof(enum resultadoEjecucion));
}
