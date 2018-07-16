#ifndef BIBLIO_H_
#define BIBLIO_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "commons/error.h"

#define ERROR -1
#define EXITO 0

typedef int socket_t;
typedef int32_t ESI_id;
typedef int32_t instancia_id;
//No creo que necesitemos más de 255 entradas de 255 caracteres...
typedef uint8_t tamEntradas_t;
typedef uint8_t cantEntradas_t;
//Tampoco creo que le den un nombre a una instancia de más de 255 caracteres...
typedef uint8_t tamNombreInstancia_t;
typedef uint8_t tamClave_t;
typedef uint8_t tamValor_t;
typedef uint8_t cantClaves_t;
typedef uint8_t booleano;


typedef struct {
	uint8_t protocolo;
} header;

/*
 * Create es para cuando se envia set por primera vez
 * a la instancia.
 */
enum instruccion {get, set, store, compactacion, create};
enum tipoDeInstruccion {bloqueante, liberadora, noDefinido};
enum resultadoEjecucion {exito, bloqueo, fin, necesitaCompactar, fallos, fNoIdentificada, fNoAccesible, fNoBloqueada, fIncrementaValor, fNoAlmaceno};
/*
 * Descripción de los estados:
 * existente - Tiene instancia asignada y un valor.
 * innexistente - No existe en el coordinador.
 * sinIniciar - Existe pero no tiene instancia asignada.
 * caida - Tiene instancia asignada pero está desconectada.
 * sinValor - Tiene instancia asignada pero está no tiene su valor.
 */
enum estadoClave {existente, innexistente, sinIniciar, caida, sinValor};

void salir_agraciadamente(int);
//-1 en error o fileDescriptor en exito
socket_t crearSocketCliente(char *, char *);
//-1 en error o fileDescriptor en exito
socket_t crearSocketServer(char *, char *);
//0 para falso o 1 para verdad
int seDesconectoSocket(socket_t);
void usarPuertoTapado (socket_t);

//-1 en error o 0 en exito
int enviarHeader (socket_t, header);
//-1 en error o 0 en exito
int enviarBuffer (socket_t, void*, int);
/*
 * Al tercer parametro de recibirMensaje
 * se le pasa un puntero por referencia
 * que se va a redireccionar a un buffer
 * en memoria creado por la funcion.
 *
 * -1 en error, 0 en exito o 1 si se cerro
 * el socket mientras recibia.
 */
int recibirMensaje (socket_t, int, void**);

/*
 * Devuelve el valor del nuevo socket aceptado.
 */
socket_t aceptarConexion(socket_t);

/*
 * Si el socket está iniciado (valor != ERROR)
 * lo cierra, no hace nada en caso contrario
 */
void cerrarSocket(socket_t);

char* resultToString(enum resultadoEjecucion);

#endif /* BIBLIO_H_ */
