/*
 ============================================================================
 Name        : coordinador.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include "coordinador.h"

t_config * configuracion = NULL;
t_list * listaInstancias = NULL;
t_list * listaEsi = NULL;
/*
 * Comentario:
 *
 * Si igual lo van a hacer primero de todo
 * (porque se hace en la inicialización),
 * tambien pueden dar el valor inicial en la
 * creación de la variable
 *
 * [MATI]
 */
int cantInstancias;
/*
 * Pregunta:
 *
 * ¿Esta variable para que la usan?
 * Acuerdense que el ID de los ESI
 * lo genera el Planificador si es
 * para eso.
 *
 * [MATI]
 */
int cantEsi;

int main(void) {

	inicializacion();

	struct sockaddr_in dirPlanificador,dirAceptado;
	int socketPlanificador, socketAceptado,socketCoordinador;
	int esPlanificador = 1;

	socketCoordinador = crearSocketServer (IPEscucha,config_get_string_value(configuracion, Puerto));

	/*
	 * Si entiendo correctamente, este while se repite
	 * hasta que se conecte el planificador, descartando
	 * las conexiones que se puedan hacer antes.
	 * Honestamente, no es la implementación que pense
	 * para esto pero no está mal así que la pueden usar
	 * si quieren pero con aclaraciones:
	 * 1- Asegurar que se ejecute una iteración y despues
	 * empezar a preguntar por la condición es exactamente
	 * lo que hace do-while
	 * 2-Por más que las conexiones anteriores no interesen
	 * si llegamos a validar es que se creo un socket y por
	 * lo tanto hay que cerrarlo en algún momento.
	 * 2.2-Con 'en algún momento' me refiero a apenas saben
	 * que no les sirve, en este caso.
	 *
	 * [MATI]
	 */
	while (esPlanificador) {
		socketPlanificador = esperarYaceptar(socketCoordinador, 2 ,&dirPlanificador);
		esPlanificador = validarPlanificador(socketPlanificador );
	}



	while (1)
	{
		socketAceptado = esperarYaceptar(socketCoordinador, 20 ,&dirAceptado);
		esESIoInstancia(socketAceptado,dirAceptado);
	}
	//close(socketCoordinador);
	exit(0);
	/*
	 * Comentario:
	 * Ese exit(0) está al pedo porque no deberia llegar
	 * en ejecución normal (ni en anormal por el while(1)
	 * pero si llegara a ser utilizable tendrian que
	 * reemplazarlo con una función que libere los recursos
	 * y despues haga exit(0), como la de error pero sin el
	 * error.
	 *
	 * [MATI]
	 */
}


void esESIoInstancia (int socketAceptado,struct sockaddr_in dir)
{

	int* mensaje;
	/*
	 * El warning ya les dice que le están pasando un int**
	 * cuando espera un void** así que por lo menos les falta
	 * el casteo. Ahora, si la idea es recibir el header
	 * ¿por qué no usar el tipo header que está definido en
	 * la biblioteca compartida?
	 *
	 * [MATI]
	 */
	int estadoDellegada = recibirMensaje(socketAceptado,4,&mensaje);

	switch(mensaje[0]){

	case 2:
		registrarInstancia(socketAceptado,mensaje);
		break;

	case 3:

		// aca ira la accion a realizarse en el caso de ser una ESI
		break;

	default:

		//Creo que no hace nada si no es Esi o Instancia
		/*
		 * No deberia pasar y no creo que lo prueben pero
		 * por ser correctos, si al socket de recepción en
		 * este punto le llega algo que no es ESI o Instancia
		 * podes cerrar ese socket y mandar un mensaje de
		 * error informando al usuario.
		 *
		 * [MATI]
		 */
		break;

	}
}

void atenderEsi(int socket){

}

void registrarEsi(int socket){

	Esi nuevaEsi;
	nuevaEsi.socket = socket;
	/*
	 * ¿Cuál es el punto de asignar el valor de una
	 * variable con basura a otra que tambien tiene basura?
	 *
	 * [MATI]
	 */
	pthread_t hiloEsi;
	nuevaEsi.socket = socket;
	nuevaEsi.hiloEsi = hiloEsi;

	if (listaEsi == NULL)
		{
			listaEsi = list_create();
		}
	else
		{
			cantEsi = list_add(listaEsi,&nuevaEsi);
		}
	/*
	 * El cuarto parametro de pthread_create es void*
	 * así que falta el casteo.
	 *
	 * [MATI]
	 *
	 * PD: Antes de que sigan este camino los invito
	 * a que vean esto piensen si no hay otra manera
	 * de resolverlo que crear un hilo por ESI.
	 * https://github.com/sisoputnfrba/foro/issues/1012
	 */
	pthread_create(&nuevaEsi.hiloEsi, NULL, (void*) atenderEsi, socket);
}

void registrarInstancia(int socket,int* mensaje)
{

	Instancia instanciaRecibida;
	instanciaRecibida.socket = socket;
	/*
	 * El warning en si les dice que le pasan
	 * un puntero de puntero en lugar de un int
	 * en el tercer parametro. Ahora:
	 * Entiendo que intentan copiar el nombre
	 * de la instancia en el struct que almacena
	 * la información pero instanciaRecibida.nombre
	 * es un puntero que va a apuntar a un espacio
	 * de memoria (porque a este punto no lo hace
	 * todavia). Revisen bien esto.
	 *
	 * [MATI]
	 */
	memcpy(&instanciaRecibida.nombre,(&mensaje) + 5,(&mensaje)+1);
	instanciaRecibida.idinstancia = cantInstancias;

	if (listaInstancias == NULL)
		{
			listaInstancias = list_create();
			cantInstancias = list_add(listaInstancias,&instanciaRecibida);
		}
	else
		{
			cantInstancias = list_add(listaInstancias,&instanciaRecibida);
		}
	/*
	 * Esto ya lo mencione en otro comentario pero
	 * lo repito más claro:
	 * 		Está mal usar un tipo de dato (por más
	 * 		que sea el mismo) cuando hay otro definido
	 * 		en la biblioteca compartida.
	 *
	 * [MATI]
	 */
	uint8_t protocolo = 5;
	int copiar;
	int * buffer = malloc(13);

	/*
	 * Relacionado al anterior.
	 * Muchos tamaños hardcodeados.
	 * ¿Qué pasa si en un momento nos damos
	 * cuenta de un byte no alcanza y usamos
	 * otro tipo de dato?
	 *
	 * [MATI]
	 */
	memcpy(buffer,&protocolo,1);

	copiar = instanciaRecibida.idinstancia;
	memcpy(buffer+1,&copiar,4);

	copiar = config_get_int_value(configuracion, "CantEntradas");
	memcpy(buffer+5,&copiar,4);

	copiar = config_get_int_value(configuracion, "TamEntradas");
	memcpy(buffer+9,&copiar,4);

	int estado = enviarBuffer (instanciaRecibida.socket, buffer, 13);

	/*
	 * Tenemos en la commons una función hecha para imprimir errores,
	 * si no la quieren usar alguna vez porque la alternativa es más
	 * facil (como acá que hay que imprimir el nombre), está bien
	 * pero usen el mismo formato.
	 *
	 * [MATI]
	 */
	if (estado != 0) {printf ("no se pudo enviar informacion de entradas a la instancia %s",instanciaRecibida.nombre ); }
}

/*
 * Más de lo mismo:
 * Es verdad que lo sockets son datos
 * de tipo int pero para ayudar a evitar
 * errores humanos tenemos definido el
 * tipo socket_t
 *
 * [MATI]
 */
int esperarYaceptar(int socketCoordinador, int colaMax,struct sockaddr_in* dir)
{
	unsigned int tam;
	listen(socketCoordinador , colaMax);
	int socket = accept(socketCoordinador, (void*)&dir, &tam);
	return socket;
	}

int validarPlanificador (int socket) {
	header* handshake = NULL;
	int estadoDellegada;
	uint8_t correcto = 1;
	/*
	 * Lo mismo que puse en el recibirMensaje anterior
	 * sobre el tercer parametro.
	 * En este caso el unico puede que hacer un casteo
	 * de void** lo arregle y deje todo funcionando y
	 * lindo.
	 *
	 * [MATI]
	 */
	estadoDellegada = recibirMensaje(socket,sizeof(header),&handshake);
	/*
	 * Primer if:
	 * 		1- El formato if(condicion)intrucciones funciona cuando
	 * 		es algo chico (if(v > 0) v++; por ejemplo) sino complica
	 * 		la lectura.
	 * 		2- Vease lo que escribi más arriba de la función para
	 * 		imprimir errores.
	 * Segundo if:
	 * 		No está "mal" pero si su idea era acortar espacio/comprimir
	 * 		podian no usar las llaves o mucho mejor, sacar el if y hacer
	 * 		return del operador ternario (?:)
	 *
	 * [MATI]
	 */
	if (estadoDellegada != 0){ printf ("no se pudo recibir header de planificador."); return 1; }
	if (handshake->protocolo != correcto){ return 1; } else {return 0;}
}


void liberarRecursos()
{
	/*
	 * Consejo:
	 * 	Cada vez que agreguen un recurso, vengan
	 * 	y ya dejan su liberación escrita.
	 *
	 * [MATI]
	 */
	if(configuracion != NULL)
		config_destroy(configuracion);
}

void salirConError(char * error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);
}

void inicializacion()
{
	configuracion = config_create(archivoConfig);
	if(configuracion == NULL)
		salirConError("Fallo al leer el archivo de configuracion del coordinador\n");
	cantInstancias = 0;

}




