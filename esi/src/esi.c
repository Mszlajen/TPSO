#include "esi.h"

t_config * configuracion = NULL;
FILE * programa = NULL;
socket_t socketCoord = ERROR, socketPlan = ERROR;
t_dictionary * tablaDeInstrucciones = NULL;

enum  instruccion * instr;
char * clave;
char * valor;

int id;
int resultadoSocket = 1;
/*
 * Esto falla porque la inicialización de una variable global solo
 * puede ser algo constante.
 * Igual, ya que estoy, escribiendo acá me parece que esto usa
 * demasiadas variables globales para un programa monohilo. Lo
 * más importante es que ande, de ultima lo arreglamos despues,
 * pero si queres anda revisandolo porque es mala practica usar
 * variables globales cuando no hace falta.
 * [MATI]
 */
void * resultEjecucion = malloc(sizeof(header));


int main(int argc, char **argv) {

	struct t_esi_operacion operacionESI;

	inicializacion(argc, argv);

	conectarConCoordinador();

	conectarConPlanificador();

	programa = abrirArchivoLectura(argv[1]);

	while (1){
		while (recibirMensaje(socketPlan,sizeof(header),"")){    //espera aviso de Ejecucion del planificador

			 /* No estoy seguro que puntero pasarle como 3er parametro
			 * a recibirMensaje.
			 * [ARIEL]
			 */

		operacionESI = leerSiguienteInstruccion(argv);

		if (operacionESI.valido){		//el ESI tiene una sentencia valida

		enviarInstruccionCoord();

		recibirRespuestaCoord();

			if (resultadoSocket == 0 && resultEjecucion == exito){
				leerSiguienteInstruccion(argv);

				send(socketPlan,resultEjecucion,sizeof(enum resultadoEjecucion),0);

				printf("La instruccion se ejecuto con exito\n");

					if (feof(programa)){

						printf("Se terminaron de ejecutar las instrucciones del ESI\n");

						break;
					}

			}else if (resultadoSocket == -1 && resultEjecucion == fallo){


				send(socketPlan,resultEjecucion,sizeof(enum resultadoEjecucion),0);

				printf("La sentencia falló\n");

			}else if (resultEjecucion == bloqueo){

			send(socketPlan,resultEjecucion,sizeof(enum resultadoEjecucion),0);

			printf("El ESI se encuentra bloqueado\n");

			}else if (resultadoSocket == 1){
				printf("Se cerró el Socket durante la ejecución.\n");


			//terminar la ejecución [MATI]
			}

			puts("Salio todo bien\n");
		}
		else {					//si el ESI tiene una sentencia invalida
			salirConError();
		}

		}

	liberarRecursos();
	exit(0);
	}
}

void inicializacion(int argc, char** argv)
{
	if(argc != 2)
		salirConError("Cantidad de parametros invalida\n");
	programa = abrirArchivoLectura(argv[1]);

	configuracion = config_create(archivoConfig);
	if(configuracion == NULL)
		salirConError("Fallo al abrir el archivo de lectura\n");

	/*
	 * Dejando de lado lo que puse más abajo sobre el pase string a instrucción.
	 * Los diccionarios y las listas de las commons reciben una dirección al dato
	 * como parametro y no el dato en sí (porque ese puntero es lo que almacenan).
	 * [MATI]
	 */
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
struct t_esi_operacion leerSiguienteInstruccion(char** argv)
{
	/*
	 * No hace falta usar un puntero para el leer caracter leido
	 * con una variable char alcanza (a fread se lo pasas usando el
	 * operador de dirección &).
	 * Y linea te combiene declararla usando string_create() para
	 * asegurarte que funcione bien con las otras funciones de las
	 * commons (porque creo que esperando un string en memoria
	 * dinamica).
	 * string_split devuelve un char** pero ademas todo el proceso de
	 * dividir la linea y demás la hace el parse vos nada más tenes que
	 * pasarle la linea y te devuelve toda la información de la instrucción
	 * [MATI]
	 */
	char * linea = string_new();
	char leido = "";
	char * operacion;

			while (leido != '\0' || !feof(programa) || leido != '\n' ){
				fread(&leido, sizeof(char),1,programa);
				string_append(*linea, leido);
			}

			operacion = string_split(*linea," ");

			return parse(operacion);
		/*
		 * Parse recibe una linea (char*) y devuelve el valor
		 * de un t_esi_operacion (definido en el parser).
		 * [MATI]
		 */
}

/*
 * Si queres dejar esto, dejalo pero para hacer una función
 * que se llama en un solo lugar y que lo unico que hace es
 * llamar otra función, bien podes escribir la otra función
 * y escribir un comentario de qué hace.
 * Tambien podes usar directamente recibirMensaje para que
 * se bloquee esperando al planificador.
 * [MATI]
 */

void enviarInstruccionCoord()
{
	int res = 0, *tamClave = sizeof(*clave), * tamValor = sizeof(*valor);
	header header;
	int * buffer;
	header.protocolo = 8;
	switch(*instr){
			/*
			 * Acá hay muchos int qué:
			 * A - No sé entiende bien que son.
			 * B - No deberian ser int, porque no usamos int
			 * para enviar información atraves de sockets.
			 * [MATI]
			 */
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

void recibirRespuestaCoord(){
	int * buffer = malloc(sizeof(header)+sizeof(int));
	resultadoSocket = recibirMensaje(socketCoord,sizeof(header)+sizeof(int),(void*) &buffer);
	memcpy(resultEjecucion, &buffer, sizeof(enum resultadoEjecucion));
}

