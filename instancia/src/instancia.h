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

cantEntradas_t encontrarEspacioLibre(tamValor_t);
void reemplazarValorMayorTamanio(char*, infoClave*, char*, tamValor_t);
void reemplazarValorIgualTamanio(char*, infoClave*, char*, tamValor_t);
void reemplazarValorMenorTamanio(char*, infoClave*, char*, tamValor_t);

void almacenarID();
cantEntradas_t tamValorACantEntradas(tamValor_t);
//Devuelve 0 en exito, ERROR si fallo
int crearMappeado(infoClave*);
int destruirMappeado(infoClave*);
int min (int, int);
void salirConError(char*);
#endif /* INSTANCIA_H_ */
