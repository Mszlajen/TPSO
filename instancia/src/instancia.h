#ifndef INSTANCIA_H_
#define INSTANCIA_H_

#include <stdio.h>
#include <sys/socket.h>
#include <pthread.h>
#include <commons/collections/list.h>
#include <biblio.h>
#include "configuracion.h"


typedef struct {
	int tiempoUltimoUso;
	uint8_t enUso;
} infoEntrada;

typedef struct {
	char* nombreClave;
	int tamanio;
	int entradaInicial;
} infoClave;


void inicializar(char*);
void conectarConCoordinador();
void recibirRespuestaHandshake();

void dump();
void procesamientoInstrucciones();

void almacenarID();
void salirConError(char*);
#endif /* INSTANCIA_H_ */
