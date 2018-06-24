#ifndef ESI_H_
#define ESI_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/error.h>
#include <biblio.h>
#include <parsi/parser.h>

#define archivoConfig "ESI.config"
#define IPCoord "IPCoord"
#define PuertoCoord "PuertoCoord"
#define IPPlan "IPPlan"
#define PuertoPlan "PuertoPlan"

void inicializacion(int, char**);
void conectarConCoordinador();
void conectarConPlanificador();

FILE* abrirArchivoLectura(char*);
void enviarHandshake (socket_t);
void liberarRecursos();
void salirConError(char*);
void esperarAvisoEjecucion();
struct t_esi_operacion leerSiguienteInstruccion(char** argv);
void enviarInstruccionCoord();
void recibirRespuestaCoord();

#endif /* ESI_H_ */
