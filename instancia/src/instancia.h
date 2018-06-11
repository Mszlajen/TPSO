#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <stdio.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <biblio.h>
#include "configuracion.h"

#define nuevoID 0;

typedef struct {
	char* clave;
} infoEntrada;

typedef struct {
	tamValor_t tamanio;
	cantEntradas_t entradaInicial;
	int tiempoUltimoUso;
	int fd;
	void* mappeado;
} infoClave;

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

enum resultadoEjecucion actualizarValorDeClave(char*, char*, tamValor_t);
enum resultadoEjecucion registrarNuevaClave(char*, char*, tamValor_t);

cantEntradas_t encontrarEspacioLibreConsecutivo(tamValor_t);
void desasociarEntradas (cantEntradas_t, cantEntradas_t);
int haySuficienteEspacio(tamValor_t);
void algoritmoDeReemplazo();
enum resultadoEjecucion actualizarValorMayorTamanio(char*, infoClave*, char*, tamValor_t);
enum resultadoEjecucion actualizarValorMenorTamanio(char*, infoClave*, char*, tamValor_t);

void reemplazoCircular();

booleano almacenarID(instancia_id);
void enviarResultadoEjecucion(socket_t, enum resultadoEjecucion);
cantEntradas_t tamValorACantEntradas(tamValor_t);
//Devuelve 0 en exito, ERROR si fallo
int crearMappeado(infoClave*);
int destruirMappeado(infoClave*);
int guardarEnArchivo(infoClave*);
tamValor_t leerDeArchivo(char*, char**);
void destruirClave(char*);
void destruirInfoClave(infoClave*);
void incrementarPunteroReemplazo();
int min (int, int);
void liberarRecursosGlobales();
void salirConError(char*);
#endif /* INSTANCIA_H_ */
