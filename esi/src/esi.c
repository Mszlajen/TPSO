#include "esi.h"

t_config * configuracion = NULL;
FILE * programa = NULL;
socket_t socketCoord = ERROR, socketPlan = ERROR;

int main(int argc, char **argv) {
	inicializacion(argc, argv);

	conectarConCoordinador();

	conectarConPlanificador();

	leerSiguienteInstruccion(argv);

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
	char * linea;
	char auxiliar;
	char resultado;
	int respuestaCoord;

	if (listen(socketPlan, 5)){

		programa = abrirArchivoLectura(argv[1]);

		for(int i=0;i<5;i++){
			while (linea != '/0' || feof(programa)){
				linea = fread(auxiliar, sizeof("cadalinea"),1,programa);  //todo: size, usar commons
				resultado = parse(linea);
			}

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
			respuestaCoord = enviarInstruccionCoord(resultado);

			if (respuestaCoord){
				//bloqueo?
			} else {
				//avisar planificador
			}

		}
	}

int enviarInstruccionCoord(char instruccion)
{
	int res = 0;
	//enviarProtocolo(socketCoord, 8);
	//res = ........ <--- todo: enviar instruccion del parametro al coordinador
	return res;
}

/*int enviarProtocolo(socket_t sock, int protocolo)
{
	header handshake;
	handshake.protocolo = protocolo;
	return enviarHeader(sock, handshake);
}
*/
