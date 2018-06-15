#include "esi.h"

t_config * configuracion = NULL;
FILE * programa = NULL;
socket_t socketCoord = ERROR, socketPlan = ERROR;
t_dictionary * tablaDeInstrucciones = NULL;

enum  instruccion * instr;
char * clave;
char * valor;

int id;

int main(int argc, char **argv) {
	inicializacion(argc, argv);

	conectarConCoordinador();

	conectarConPlanificador();

	programa = abrirArchivoLectura(argv[1]);

	leerSiguienteInstruccion(argv);

	enviarInstruccionCoord();

	esperarRespuestaCoord();

	puts("Salio todo bien\n");
	liberarRecursos();
	exit(0);
}

void inicializacion(int argc, char** argv)
{
	if(argc != 2)
		salirConError("Cantidad de parametros invalida\n");
	programa = abrirArchivoLectura(argv[1]);

	configuracion = config_create(archivoConfig);
	if(configuracion == NULL)
		salirConError("Fallo al abrir el archivo de lectura\n");

	dictionary_put(tablaDeInstrucciones,"GET",get);
	dictionary_put(tablaDeInstrucciones,"SET", set);
	dictionary_put(tablaDeInstrucciones,"STORE", store);
}

void conectarConCoordinador()
{
	char * IP = config_get_string_value(configuracion, IPCoord);
	char * puerto = config_get_string_value(configuracion, PuertoCoord);
	socketCoord = crearSocketCliente(IP, puerto);
	if(socketCoord == ERROR)
		salirConError("No se pudo conectar al coordinador\n");
	enviarHandshake(socketCoord);
}

void conectarConPlanificador()
{
	char * IP = config_get_string_value(configuracion, IPPlan);
	char * puerto = config_get_string_value(configuracion, PuertoPlan);
	socketPlan = crearSocketCliente(IP, puerto);
	if(socketPlan == ERROR)
		salirConError("No se pudo conectar al coordinador\n");
	enviarHandshake(socketPlan);
	recibirMensaje(socketPlan,sizeof(int),(void*) &id);
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
		salirConError("No se pudo enviar mensaje handshake\n");
}

void salirConError(char* error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}

void liberarRecursos()
{
	if(configuracion != NULL)
		config_destroy(configuracion);
	if(programa != NULL)
		fclose(programa);
	if(socketCoord != ERROR)
		cerrarSocket(socketCoord);
	if(socketPlan != ERROR)
		cerrarSocket(socketPlan);
}

/*
 * No te olvides de poner las declaraciones en el .h
 * porque si no va fallar en tiempo de ejecución
 * [MATI]
 *
 * PD: Si sos de los que escriben todo y despues
 * lo declaran, ignora esto.
 */
void leerSiguienteInstruccion(char** argv)
{
	/*
	 * Listen devuelve 1 o 0 para error y exito respectivamente,
	 * no la cantidad de peticiones encoladas.
	 * [MATI]
	 */
	char * linea = "";
	char * leido = "";
	char * operacion;

			while (*leido != '/0' || !feof(programa) || *leido != '\n' ){
				fread(leido, sizeof(char),1,programa);
				string_append(*leido, *linea);
			}
			operacion = string_split(*leido," ");

			instr = dictionary_get(tablaDeInstrucciones,operacion[0]);
			clave = operacion[1];
			valor = operacion[2];
		/*
		 * Lo que tenes que leer no tiene un tamaño fijo sino
		 * que tenes que leer hasta encontrar el caracter de
		 * fin de linea o fin de archivo, lo que pase primero.
		 *
		 * Ademas el primer paramentro de fread recibe la
		 * dirección de memoria donde se va a guardar la
		 * informacion.
		 * [MATI]
		 *
		 * PD: para solucionar esto va a tener que usar la
		 * biblioteca de strings de las commons.
		 */

		/*
		 * Parse recibe una linea (char*) y devuelve el valor
		 * de un t_esi_operacion (definido en el parser).
		 * [MATI]
		 */
	}

void enviarInstruccionCoord()
{
	int res = 0, *tamClave = sizeof(*clave), * tamValor = sizeof(*valor);
	header header;
	int * buffer;
	header.protocolo = 8;
	switch(*instr){
			case get:
					buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(int) * 3);

					memcpy(buffer , &header.protocolo , sizeof(header));
					memcpy(buffer+sizeof(header) , &id , sizeof(int) );
					memcpy(buffer+sizeof(header) , &instr , sizeof(enum instruccion) );
					memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(enum instruccion) , tamClave , sizeof(int) );
					memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(enum instruccion)+sizeof(int), &clave , *tamClave );
					memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(enum instruccion)+sizeof(int)+*tamClave , tamValor , sizeof(int) );
					memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(enum instruccion)+sizeof(int)*2+*tamClave , &valor , *tamValor );

					enviarBuffer (socketCoord, buffer , sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int)*3 + *tamClave + *tamValor );
					free(buffer);
				break;
			case set:
					buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(int)*2);

					memcpy(buffer , &header.protocolo , sizeof(header) );
					memcpy(buffer+sizeof(header) , &id , sizeof(int) );
					memcpy(buffer+sizeof(int)+sizeof(header) , &instr , sizeof(enum instruccion) );
					memcpy(buffer+sizeof(int)+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(int) );
					memcpy(buffer+sizeof(int)+sizeof(header)+sizeof(enum instruccion)+sizeof(int), &clave , *tamClave );

					enviarBuffer (socketCoord, buffer , sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int)*2 + *tamClave );
					free(buffer);
				break;
			case store:
					buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(int)*2);

					memcpy(buffer , &header.protocolo , sizeof(header) );
					memcpy(buffer+sizeof(header) , &id , sizeof(int) );
					memcpy(buffer+sizeof(int)+sizeof(header) , &instr , sizeof(enum instruccion) );
					memcpy(buffer+sizeof(int)+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(int) );
					memcpy(buffer+sizeof(int)+sizeof(header)+sizeof(enum instruccion)+sizeof(int), &clave , *tamClave );

					enviarBuffer (socketCoord, buffer , sizeof(header) +sizeof(enum tipoDeInstruccion) + sizeof(int)*2 + *tamClave );
					free(buffer);
				break;
				}
}

void esperarRespuestaCoord(){
	int * buffer = malloc(sizeof(header)+sizeof(int));
	recibirMensaje(socketCoord,sizeof(header)+sizeof(int),(void*) &buffer);

}

