#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <stdio.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <biblio.h>
#include "configuracion.h"

#define nuevoID 0;


typedef struct {
	char* clave;
} infoEntrada_t;

typedef struct {
	tamValor_t tamanio;
	cantEntradas_t entradaInicial;
	int tiempoUltimoUso;
	int fd;
	void* mappeado;
} infoClave_t;

typedef struct {
	enum instruccion tipo;
	char* clave;
	tamValor_t tamValor;
	char* valor;
} instruccion_t;

void inicializar(char*, socket_t*);
void conectarConCoordinador(socket_t*);
void recibirRespuestaHandshake(socket_t);

void dump();
void procesamientoInstrucciones(socket_t);


instruccion_t* recibirInstruccionCoordinador(socket_t);
void incrementarUltimoUsoClaves();
enum resultadoEjecucion instruccionSet(instruccion_t*);
enum resultadoEjecucion instruccionStore(instruccion_t*);
enum resultadoEjecucion instruccionCompactacion();

enum resultadoEjecucion actualizarValorDeClave(char*, char*, tamValor_t);
enum resultadoEjecucion registrarNuevaClave(char*, char*, tamValor_t);

cantEntradas_t encontrarEspacioLibreConsecutivo(tamValor_t);
void asociarEntradas (cantEntradas_t, cantEntradas_t, char*);
cantEntradas_t obtenerEntradasDisponibles();
void algoritmoDeReemplazo();
enum resultadoEjecucion actualizarValorMayorTamanio(char*, infoClave_t*, char*, tamValor_t);
enum resultadoEjecucion actualizarValorMenorTamanio(char*, infoClave_t*, char*, tamValor_t);

infoClave_t* reemplazoCircular();
infoClave_t* reemplazoLRU();
infoClave_t* reemplazoBSU();

infoClave_t* desempatePorCircular(t_list*);

booleano almacenarID(instancia_id);
void enviarResultadoEjecucion(socket_t, enum resultadoEjecucion);
cantEntradas_t tamValorACantEntradas(tamValor_t);
//Devuelve 0 en exito, ERROR si fallo
int crearMappeado(infoClave_t*);
int destruirMappeado(infoClave_t*);
int guardarEnArchivo(infoClave_t*);
tamValor_t leerDeArchivo(char*, char**);
void destruirClave(char*);
void destruirInfoClave(infoClave_t*);
void incrementarPunteroReemplazo();
int min (int, int);
void liberarRecursosGlobales();
void salirConError(char*);
#endif /* INSTANCIA_H_ */
