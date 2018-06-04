#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <stdio.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <pthread.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <biblio.h>
#include "configuracion.h"

typedef struct {
	int tiempoUltimoUso;
	char* clave;
} infoEntrada;

typedef struct {
	tamValor_t tamanio;
	cantEntradas_t entradaInicial;
	FILE* archivo;
	void* mappeado;
} infoClave;

typedef struct {
	enum instruccion tipo;
	char* clave;
	tamValor_t tamValor;
	char* valor;
} instruccion_t;


void inicializar(char*);
void conectarConCoordinador();
void recibirRespuestaHandshake();

void dump();
void procesamientoInstrucciones();


instruccion_t* recibirInstruccionCoordinador();
void incrementarUltimoUsoEntradas();
void instruccionSet(instruccion_t*);
void instruccionStore(instruccion_t*);

void actualizarValorDeClave(char*, char*, tamValor_t);
void registrarNuevaClave(char*, char*, tamValor_t);

cantEntradas_t encontrarEspacioLibreConsecutivo(tamValor_t);
int haySuficienteEspacio(tamValor_t);
void algoritmoDeReemplazo();
void actualizarValorMayorTamanio(char*, infoClave*, char*, tamValor_t);
void actualizarValorIgualTamanio(char*, infoClave*, char*, tamValor_t);
void actualizarValorMenorTamanio(char*, infoClave*, char*, tamValor_t);

void reemplazoCircular();

void almacenarID();
void enviarResultadoEjecucion(enum resultadoEjecucion);
cantEntradas_t tamValorACantEntradas(tamValor_t);
//Devuelve 0 en exito, ERROR si fallo
int crearMappeado(infoClave*);
int destruirMappeado(infoClave*);
void destruirClave(char*);
void destruirInfoClave(infoClave*);
void incrementarPunteroReemplazo();
int min (int, int);
void liberarRecursosGlobales();
void salirConError(char*);
#endif /* INSTANCIA_H_ */
