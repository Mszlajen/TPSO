#ifndef ESI_H_
#define ESI_H_

#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/error.h>
#include <biblio.h>
#include <parsi/parser.h>

#define IPCoord "IPCoord"
#define PuertoCoord "PuertoCoord"
#define IPPlan "IPPlan"
#define PuertoPlan "PuertoPlan"

void inicializacion(int, char**, t_config**, FILE**);
socket_t conectarConCoordinador(t_config*);
socket_t conectarConPlanificador(t_config*);

FILE* abrirArchivoLectura(char*);
void enviarHandshake (socket_t);
void informarError(enum resultadoEjecucion, ESI_id);
void liberarRecursos();
void salirConError(char*);
// Devuelve 0 si no hay más instrucciones para leer, y usa el segundo parametro como salida en caso contrario
booleano leerSiguienteInstruccion(FILE*, t_esi_operacion*);
void enviarInstruccionCoord(socket_t, t_esi_operacion, ESI_id);
void enviarResultadoPlanificador(socket_t, enum resultadoEjecucion, booleano);

#endif /* ESI_H_ */
