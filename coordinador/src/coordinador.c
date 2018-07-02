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
t_list * listaClaves = NULL;
t_dictionary * tablaDeClaves = NULL;

socket_t  socketPlanificador;
socket_t  socketCoordinador;
socket_t  socketConsultaClave;

/*
 * Primero hago está aclaración, por lo que encontre de pthread_cond
 * son más parecidos a los monitores que a los semaforos.
 * Segundo, me parece que no estás entendiendo bien como usarlos, ahora
 * mismo todos los hilos de instancias se quedan esperando por el mismo
 * cond y no hay manera de decirle a la instancia particular que devolvio
 * el algoritmo que se desbloquee (El cond te sirve para esto, pero no de
 * está manera).
 * Tambien me parece que hay recursos sincronizados de más pero no estoy seguro.
 * [MATI]
 */

pthread_mutex_t mInstanciaActual, mEsiActual, mRespuestaInstancia, mConsultarPorClave , mListaInst, mAuxiliarIdInstancia;
pthread_cond_t sbInstanciaActual, sbEsiActual, sbRespuestaInstancia, sbConsultarPorClave , sbRespuestaPlanificador;


char * Algoritmo;
int auxiliarIdInstancia;
int auxiliarIdEsi;
int contadorEquitativeLoad = 0;
int contadorIdInstancias = 1;
int cantInstancias = 0;
int respuestaPlanificador;
int hayCompactacion = 0;
int instanciasCompactadas= 0;
int hayQueSimular = 0;
int hayStatusEnIdInstancia = 0;
Instancia * instanciaActual;
t_log * logOperaciones;
Esi * esiActual;
RespuestaStatus RespStatus;

int main(int argc, char **argv) {

	int esPlanificador;
	pthread_t hiloRecibeConexiones;

	inicializacion(argv[1]);

	socketCoordinador = crearSocketServer (config_get_string_value(configuracion,IPEscucha),config_get_string_value(configuracion, Puerto));

	 do {
		socketPlanificador = esperarYaceptar(socketCoordinador);
		esPlanificador = validarPlanificador(socketPlanificador );
	} while (esPlanificador);

	pthread_create(&hiloRecibeConexiones, NULL, (void*) recibirConexiones, NULL);

	pthread_join(hiloRecibeConexiones, NULL);

	liberarRecursos();
	exit(0);

}

void hiloPlanificador (socket_t socket){


	int maxFd = max(socketPlanificador, socketConsultaClave);
	fd_set master, read;

	FD_ZERO(&master);
	FD_SET(socketPlanificador, &master);
	FD_SET(socketConsultaClave, &master);
	while(1)
	{
		read = master;
		select(maxFd, &read, NULL, NULL, NULL);

		if(FD_ISSET(socketPlanificador, &read))
		{
			tratarStatusPlanificador();

		}else if(FD_ISSET(socketConsultaClave, &read)){

			consultarPorClave();
		}
	}
}

void hiloInstancia (Instancia * instancia) {
	header header;

	while(1){

	if( comprobarEstadoDeInstancia(instancia) ){
		break;
	}
		pthread_mutex_lock(&mInstanciaActual);

		pthread_cond_wait(&sbInstanciaActual,&mInstanciaActual);

		if(hayStatusEnIdInstancia == instancia->idinstancia ){

			tratarStatusInstancia(instancia);

		}else{
			if(hayCompactacion){

						header.protocolo = 11;
						void * buffer;
						enum instruccion compac = compactacion ;
						int estado;

						buffer = malloc(sizeof(header) + sizeof(enum instruccion));

						memcpy(buffer , &header.protocolo , sizeof(header) );
						memcpy(buffer+sizeof(header) , &compac , sizeof(enum instruccion) );

						estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) );

						while(estado !=0){
							error_show("no se pudo comunicar compactacion a la instancia, volviendo a intentar");
							estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) );
						}
						free(buffer);

						instanciasCompactadas++;
						if(instanciasCompactadas == cantInstancias){
							hayCompactacion = 0;
							instanciasCompactadas = 0;
						}


					}else{


						if(instanciaActual->idinstancia == instancia->idinstancia){

							if( comprobarEstadoDeInstancia(instancia) ){
									break;
								}

								switch(instancia->esiTrabajando->instr){
									case get:
										break;
									case set:
										tratarSetInstancia(instancia);
										break;
									case store:
										tratarStoreInstancia(instancia);
										break;
									case compactacion:
										break;
									case create:
										break;
									}

								pthread_mutex_lock(&mRespuestaInstancia);
								if( comprobarEstadoDeInstancia(instancia) ){
										break;
									}
								recibirResultadoInstancia(instancia);
								pthread_mutex_unlock(&mRespuestaInstancia);
								pthread_cond_signal(&sbRespuestaInstancia);
							}
						pthread_mutex_unlock(&mInstanciaActual);

					}
				}

		}

}


void hiloEsi (Esi * esi) {
	tamClave_t * tamClave = NULL;
	tamValor_t * tamValor = NULL;
	header * header;
	enum instruccion * instr = NULL;

	while(1){

	if( recibirMensaje(esi->socket,sizeof(header),(void *) &header) != 1 ){
		recibirMensaje(esi->socket,sizeof(int),(void *) &esi->idEsi);

		if (header->protocolo == 8){
				recibirMensaje(esi->socket,sizeof(enum instruccion),(void *) &instr );
				esi->instr = *instr;

				switch(esi->instr){
						case get:
							recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
							recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

							tratarGetEsi(esi);
							enviarResultadoEsi(esi);
							break;
						case set:
							recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
							recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);
							recibirMensaje(esi->socket,sizeof(tamValor_t),(void *) &tamValor );
							recibirMensaje(esi->socket,*tamValor,(void *) &esi->valor);

							pthread_mutex_lock(&mRespuestaInstancia);
							tratarSetEsi(esi);
							pthread_cond_wait(&sbRespuestaInstancia,&mRespuestaInstancia);

							if(esi->res == necesitaCompactar){
								hayCompactacion = 1;
								pthread_cond_broadcast(&sbInstanciaActual); // signal a todos los hilos de instancia para que revisen si deben compactar

								while(instanciasCompactadas != cantInstancias){} //espera activa

								pthread_cond_broadcast(&sbInstanciaActual); // signal a todos los de instancia para que revisen si les toca ejecutar
								pthread_cond_wait(&sbRespuestaInstancia,&mRespuestaInstancia);
							}

							pthread_mutex_unlock(&mRespuestaInstancia);
							enviarResultadoEsi(esi);
							break;
						case store:
							recibirMensaje(esi->socket,sizeof(tamClave_t),(void *) &tamClave );
							recibirMensaje(esi->socket,*tamClave,(void *) &esi->clave);

							pthread_mutex_lock(&mRespuestaInstancia);
							tratarStoreEsi(esi);
							pthread_cond_wait(&sbRespuestaInstancia,&mRespuestaInstancia);
							pthread_mutex_unlock(&mRespuestaInstancia);
							enviarResultadoEsi(esi);
							break;
						case create:
							break;
						case compactacion:
							break;
						}
				free(header);
				free(instr);
			}
		}else{
			error_show("se desconecto socket del esi, no se pudo recibir la instruccion ");

			auxiliarIdEsi = esi->idEsi;
			list_remove_by_condition(listaEsi, (void*)buscarEsiPorId);
			free(esi);
		}
	}
}

void tratarGetEsi(Esi * esi){

	if( dictionary_has_key(tablaDeClaves, esi->clave) ){


		esiActual = esi;
		pthread_mutex_lock(&mConsultarPorClave);
		pedirConsultaClave();

		pthread_cond_wait(&sbRespuestaPlanificador,&mConsultarPorClave);
		pthread_mutex_unlock(&mConsultarPorClave);

		if(respuestaPlanificador){
			esi->res = bloqueo;
		}else{
			esi->res = exito;
		}


	}
	else
	{

		esiActual = esi;

		pthread_mutex_lock(&mConsultarPorClave);
		pedirConsultaClave();

		pthread_cond_wait(&sbRespuestaPlanificador,&mConsultarPorClave);
		pthread_mutex_unlock(&mConsultarPorClave);

		if(respuestaPlanificador){
			esi->res = bloqueo;
		}else{
			esi->res = exito;
		}

		list_add(listaClaves,esi->clave);



	}
}


void tratarStoreInstancia(Instancia * instancia) {
	tamClave_t * tamClave = NULL;
	header header;
	header.protocolo = 11;
	void * buffer;
	int estado;

	buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t));

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &instancia->esiTrabajando->instr , sizeof(enum instruccion) );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(tamClave_t) );
	memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t), &instancia->esiTrabajando->clave , *tamClave );

	estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) );

	while(estado !=0){
		error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) );
	}
	free(buffer);

	log_info(logOperaciones, "ESI %i STORE %s " , instancia->esiTrabajando->idEsi , instancia->esiTrabajando->clave);
}

void tratarStoreEsi(Esi * esi) {
	Instancia * instancia;

	esiActual = esi;

	pthread_mutex_lock(&mConsultarPorClave);
	pedirConsultaClave();

	pthread_cond_wait(&sbRespuestaPlanificador,&mConsultarPorClave);
	pthread_mutex_unlock(&mConsultarPorClave);

	if(respuestaPlanificador){
		esi->res = fallo;
	}else{
		instancia = dictionary_get(tablaDeClaves, esi->clave);
		instancia->esiTrabajando = esi;
		pthread_mutex_lock(&mInstanciaActual);
		instanciaActual = instancia;
		pthread_mutex_unlock(&mInstanciaActual);
		pthread_cond_broadcast(&sbInstanciaActual);
	}

}

void tratarSetEsi(Esi * esi) {
	Instancia * instancia;

	esiActual = esi;

	pthread_mutex_lock(&mConsultarPorClave);
	pedirConsultaClave();

	pthread_cond_wait(&sbRespuestaPlanificador,&mConsultarPorClave);
	pthread_mutex_unlock(&mConsultarPorClave);

	if(respuestaPlanificador){
		//este es el caso en que la instancia no tiene la clave, el coordinador le retorna un fallo
		esi->res = fallo;
	}else{
		instancia = dictionary_get(tablaDeClaves, esi->clave);

		if(instancia == NULL){
			instancia = algoritmoUsado();
			dictionary_put(tablaDeClaves, esi->clave, &instancia);

			esi->instr = create;

			instancia->esiTrabajando = esi;
			pthread_mutex_lock(&mInstanciaActual);
			instanciaActual = instancia;
			pthread_mutex_unlock(&mInstanciaActual);
			pthread_cond_broadcast(&sbInstanciaActual);

		}else{

			instancia->esiTrabajando = esi;
			pthread_mutex_lock(&mInstanciaActual);
			instanciaActual = instancia;
			pthread_mutex_unlock(&mInstanciaActual);
			pthread_cond_broadcast(&sbInstanciaActual);

		}
	}

}

void tratarSetInstancia(Instancia * instancia){
	tamClave_t * tamClave = NULL;
	tamValor_t * tamValor = NULL;
	header header;
	header.protocolo = 11;
	void * buffer;
	int estado;

		buffer = malloc(sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(tamClave_t) + sizeof(tamValor_t));

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &instancia->esiTrabajando->instr , sizeof(enum instruccion) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion) , tamClave , sizeof(tamClave_t) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t), &instancia->esiTrabajando->clave , *tamClave );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t)+*tamClave , tamValor , sizeof(tamValor_t) );
		memcpy(buffer+sizeof(header)+sizeof(enum instruccion)+sizeof(tamClave_t)+*tamClave+sizeof(tamValor_t), &instancia->esiTrabajando->valor , *tamValor );

		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + *tamValor + sizeof(tamClave_t) + sizeof(tamValor_t));

		while(estado !=0){
			error_show("no se pudo comunicar el store a la instancia, volviendo a intentar");
			estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(enum instruccion) + *tamClave + sizeof(tamClave_t) + sizeof(tamValor_t) );
			}

		free(buffer);

	log_info(logOperaciones, "ESI %i SET %s %s " , instancia->esiTrabajando->idEsi ,instancia->esiTrabajando->clave ,instancia->esiTrabajando->valor);
}

void pedirConsultaClave(){
	int * buffer = malloc(sizeof(int));
	*buffer = 1;
	enviarBuffer (socketConsultaClave , buffer , sizeof(int) );
	free(buffer);
}

void consultarPorClave(){
	header header;
	header.protocolo = 9;
	int *respuesta;
	enum tipoDeInstruccion tipo;

	tamClave_t tamClave = string_length(esiActual->clave) + sizeof("\0");

	switch(esiActual->instr){
	case store:
		tipo = liberadora;
		break;
	case get:
		tipo = bloqueante;
		break;
	case set:
		tipo = noDefinido;
		break;
	case compactacion:
		break;
	case create:
		break;
	}

	void * buffer = malloc(sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) + tamClave);

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &esiActual->idEsi, sizeof(ESI_id) );
	memcpy(buffer+sizeof(header) + sizeof(ESI_id) , &tipo, sizeof(enum tipoDeInstruccion) );
	memcpy(buffer+sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion)  , &tamClave, sizeof(tamClave_t) );
	memcpy(buffer+sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) , &esiActual->clave, tamClave );

	int estado = enviarBuffer (socketPlanificador , buffer , sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) + tamClave);

	while(estado !=0){
		error_show("no se pudo preguntar por el estado de la clave al planificador, volviendo a intentar");
		estado = enviarBuffer (socketPlanificador , buffer , sizeof(header) + sizeof(ESI_id) + sizeof(enum tipoDeInstruccion) + sizeof(tamClave_t) + tamClave);
		}
	free(buffer);

	respuesta = malloc(sizeof(booleano));
	recibirMensaje(socketPlanificador,sizeof(header),(void *) &header);
	recibirMensaje(socketPlanificador,sizeof(int),(void *) &respuesta);

	respuestaPlanificador = *respuesta;

	pthread_cond_signal(&sbRespuestaPlanificador);

	free(respuesta);
}

void tratarStatusPlanificador () {
	Instancia * instancia;
	header * header;
	int estado;

	recibirMensaje(socketPlanificador,sizeof(header),(void *) &header);
	if(header->protocolo == 13){
		recibirMensaje(socketPlanificador,sizeof(tamClave_t),(void *) &RespStatus.tamClave );
		recibirMensaje(socketPlanificador,*RespStatus.tamClave,(void *) &RespStatus.clave);

		instancia = dictionary_get(tablaDeClaves,RespStatus.clave);

		pthread_mutex_lock(&mRespuestaInstancia);

		if(instancia == NULL){
			hayQueSimular = 1;
			instancia = algoritmoUsado();
			RespStatus.existe = 0;
			RespStatus.estado = innexistente;
		}else if(seDesconectoSocket(instancia->socket) ){
				RespStatus.existe = 0;
				RespStatus.estado = caida;
			}else{
				hayStatusEnIdInstancia = instancia->idinstancia;
				pthread_cond_broadcast(&sbInstanciaActual); // signal a todos los hilos de instancia para que revisen si deben consultar por status
				pthread_cond_wait(&sbRespuestaInstancia,&mRespuestaInstancia);
			}

		if(*RespStatus.existe == 1){
			// logre conseguir el valor de la clave y lo mando
			header->protocolo = 14;
			tamNombreInstancia_t tamNombre = sizeof(*instancia->nombre);

			void * buffer = malloc(sizeof(header)
					+ sizeof(instancia_id)
					+ tamNombre
					+ sizeof(tamNombreInstancia_t)
					+ sizeof(tamValor_t)
					+ *RespStatus.tamValor)
					+ sizeof(enum estadoClave);

			memcpy(buffer , &header , sizeof(header) );
			memcpy(buffer+sizeof(header) , &instancia->idinstancia , sizeof(instancia_id) );
			memcpy(buffer+sizeof(header)+sizeof(instancia_id) , &tamNombre, sizeof(tamNombreInstancia_t) );
			memcpy(buffer+sizeof(header)+sizeof(instancia_id)+sizeof(tamNombreInstancia_t) , &instancia->nombre, tamNombre );
			memcpy(buffer+sizeof(header)+sizeof(instancia_id)+sizeof(tamNombreInstancia_t)+tamNombre , &RespStatus.estado, sizeof(enum estadoClave));
			memcpy(buffer+sizeof(header)+sizeof(instancia_id)+sizeof(tamNombreInstancia_t)+tamNombre+sizeof(enum estadoClave) , &RespStatus.tamValor, sizeof(tamValor_t));
			memcpy(buffer+sizeof(header)+sizeof(instancia_id)+sizeof(tamNombreInstancia_t)+tamNombre+sizeof(enum estadoClave)+sizeof(tamValor_t), &RespStatus.valor, *RespStatus.tamValor );

			estado = enviarBuffer (instancia->socket , buffer , sizeof(header)+sizeof(instancia_id)+tamNombre+sizeof(tamNombreInstancia_t)+sizeof(tamValor_t)+*RespStatus.tamValor+sizeof(enum estadoClave));

			while(estado !=0){
				error_show("no se pudo comunicar respuesta status al Planificador, volviendo a intentar");
				estado = enviarBuffer (instancia->socket , buffer , sizeof(header)+sizeof(instancia_id)+tamNombre+sizeof(tamNombreInstancia_t)+sizeof(tamValor_t)+*RespStatus.tamValor+sizeof(enum estadoClave));
			}

			free(buffer);

		}else{
			// no logre conseguir el valor de la clave, no lo envio
			header->protocolo = 14;
			tamNombreInstancia_t tamNombre = sizeof(*instancia->nombre);

			void * buffer = malloc(sizeof(header)+sizeof(instancia_id)+tamNombre+sizeof(tamNombreInstancia_t)+sizeof(tamValor_t)+sizeof(enum estadoClave));

			memcpy(buffer , &header , sizeof(header) );
			memcpy(buffer+sizeof(header) , &instancia->idinstancia , sizeof(instancia_id) );
			memcpy(buffer+sizeof(header)+sizeof(instancia_id) , &tamNombre, sizeof(tamNombreInstancia_t) );
			memcpy(buffer+sizeof(header)+sizeof(instancia_id)+sizeof(tamNombreInstancia_t) , &instancia->nombre, tamNombre );
			memcpy(buffer+sizeof(header)+sizeof(instancia_id)+sizeof(tamNombreInstancia_t)+tamNombre , &RespStatus.estado, sizeof(enum estadoClave));


			estado = enviarBuffer (instancia->socket , buffer , sizeof(header)+sizeof(instancia_id)+tamNombre+sizeof(tamNombreInstancia_t)+sizeof(tamValor_t) +sizeof(enum estadoClave));

			while(estado !=0){
				error_show("no se pudo comunicar respuesta status al Planificador, volviendo a intentar");
				estado = enviarBuffer (instancia->socket , buffer , sizeof(header)+sizeof(instancia_id)+tamNombre+sizeof(tamNombreInstancia_t)+sizeof(tamValor_t)+sizeof(enum estadoClave) );
			}

			free(buffer);
		}

		pthread_mutex_unlock(&mRespuestaInstancia);
	}
}

void tratarStatusInstancia (Instancia * instancia) {
	header header;
	void * buffer ;
	header.protocolo = 13;
	int estado;

	buffer = malloc(sizeof(header) + sizeof(tamClave_t) + *RespStatus.tamClave );

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &RespStatus.tamClave , sizeof(tamClave_t) );
	memcpy(buffer+sizeof(header)+sizeof(tamClave_t) , &RespStatus.clave, *RespStatus.tamClave );

	estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(tamClave_t) + *RespStatus.tamClave );

	while(estado !=0){
		error_show("no se pudo preguntar por valor a la instancia, volviendo a intentar");
		estado = enviarBuffer (instancia->socket , buffer , sizeof(header) + sizeof(tamClave_t) + *RespStatus.tamClave );
	}

	free(buffer);

	hayStatusEnIdInstancia = 0;

	recibirMensaje(instancia->socket,sizeof(header),(void *) &header );
	if(header.protocolo == 15){
		recibirMensaje(instancia->socket,sizeof(booleano),(void *) &RespStatus.existe);
		if(*RespStatus.existe == 0){
			RespStatus.estado = sinValor;
			pthread_cond_signal(&sbRespuestaInstancia);
		}else{
			recibirMensaje(instancia->socket,sizeof(tamValor_t),(void *) &RespStatus.tamValor);
			recibirMensaje(instancia->socket,*RespStatus.tamValor,(void *) &RespStatus.valor );
			RespStatus.estado = existente;
			pthread_cond_signal(&sbRespuestaInstancia);
		}

	}
}

int comprobarEstadoDeInstancia (Instancia * instancia){

	if( seDesconectoSocket(instancia->socket) ){

		pthread_mutex_lock(&mAuxiliarIdInstancia);
		auxiliarIdInstancia = instancia->idinstancia;
		list_remove_by_condition(listaInstancias, (void*)buscarInstanciaPorId );
		free(instancia);
		pthread_mutex_unlock(&mAuxiliarIdInstancia);
		cantInstancias--;

		return 1;
		}else{
		return 0;
		}


}

void recibirResultadoInstancia(Instancia * instancia){
	header * header;
	int * resultado;
	int estadoInstancia;
	cantEntradas_t * entradas;

	estadoInstancia = recibirMensaje(instancia->socket,sizeof(header),(void *) &header);

	if(header->protocolo == 12){
		recibirMensaje(instancia->socket,sizeof(enum resultadoEjecucion),(void *) &resultado );

		if(instancia->esiTrabajando->instr == set || instancia->esiTrabajando->instr == create ){
			recibirMensaje(instancia->socket,sizeof(cantEntradas_t),(void *) &entradas);
			instancia->entradasLibres = *entradas;
		}

		if (estadoInstancia == 1){
			*resultado = fallo; // esto sucede cuando la instancia se cae durante la ejecucion y el esi debe abortar
		}

		switch(*resultado){
		case exito:
			instancia->esiTrabajando->res = exito;
			break;
		case fallo:
			instancia->esiTrabajando->res = fallo;
			break;
		case necesitaCompactar:
			instancia->esiTrabajando->res = necesitaCompactar;
			break;
		}
	}
	free(resultado);
}

void enviarResultadoEsi(Esi * esi){

	header header;
	header.protocolo = 12;
	void * buffer;
	int enviado;

	buffer = malloc(sizeof(header) + sizeof(enum resultadoEjecucion) );

	memcpy(buffer , &header.protocolo , sizeof(header) );
	memcpy(buffer+sizeof(header) , &esi->res , sizeof(enum resultadoEjecucion));

	enviado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );

	while(enviado != 0){
		error_show ("no se envio correctamente el informe de resultado al esi, enviando nuevamente");
		enviado = enviarBuffer (esi->socket , buffer , sizeof(header) + sizeof(enum resultadoEjecucion) );
	}
	free(buffer);
}

void setearEsiActual(Esi esi){
	pthread_mutex_lock(&mEsiActual);
	esiActual->instr = esi.instr;
	esiActual->idEsi = esi.idEsi;
	esiActual->socket = esi.socket;
	strcpy(esiActual->clave,esi.clave);
	pthread_mutex_unlock(&mEsiActual);
}

void recibirConexiones() {

	socket_t socketAceptado;
	while (1)
		{
			socketAceptado = esperarYaceptar(socketCoordinador);
			esESIoInstancia(socketAceptado);
		}
}

void esESIoInstancia (socket_t socketAceptado)
{

	header* Header;
	recibirMensaje(socketAceptado,sizeof(header),(void *) &Header);

	switch(Header->protocolo){
	case 2:
		registrarInstancia(socketAceptado);
		break;
	case 3:
		registrarEsi(socketAceptado);
		break;
	default:
		break;
	}
}

void registrarEsi(socket_t socket){

	Esi * nuevaEsi = malloc(sizeof(Esi));
	nuevaEsi->socket = socket;
	nuevaEsi->clave = NULL;
	nuevaEsi->valor = NULL;
	list_add(listaEsi,nuevaEsi);

	pthread_t hiloEsi;
	pthread_create(&hiloEsi, NULL, (void*) hiloEsi, nuevaEsi);
	//TODO hacer free en algun lugar
}

void registrarInstancia(socket_t socket)
{
	tamNombreInstancia_t * tamNombre = NULL;
	instancia_id * idInstancia;
	Instancia * instanciaRecibida = malloc(sizeof(Instancia));
	instanciaRecibida->esiTrabajando = NULL;
	int enviado;
	pthread_t hiloInstancia;


	recibirMensaje(socket,sizeof(int),(void*) &idInstancia );

	if(*idInstancia == 0){

		recibirMensaje(socket,sizeof(int),(void *) &tamNombre );
		recibirMensaje(socket,*tamNombre,(void *) &instanciaRecibida->nombre );

		instanciaRecibida->idinstancia = contadorIdInstancias;
		instanciaRecibida->socket = socket;


		contadorIdInstancias ++;

		pthread_mutex_lock(&mListaInst);
		cantInstancias = list_add(listaInstancias,instanciaRecibida);
		pthread_mutex_unlock(&mListaInst);

		header header;
		header.protocolo = 5;
		cantEntradas_t cantEntradas = config_get_int_value(configuracion, "CantEntradas");
		tamEntradas_t tamEntradas = config_get_int_value(configuracion, "TamEntradas");

		instanciaRecibida->entradasLibres = cantEntradas;

		int * buffer = malloc(sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) );

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &instanciaRecibida->idinstancia , sizeof(int) );
		memcpy(buffer+sizeof(header)+sizeof(int) , &cantEntradas , sizeof(cantEntradas_t) );
		memcpy(buffer+sizeof(header)+sizeof(int)+sizeof(cantEntradas_t) , &tamEntradas , sizeof(tamEntradas_t) );

		enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) );


		while(enviado != 0){
			error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
			enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(instanciaRecibida->idinstancia) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) );
		}
		//TODO hacer free en algun lugar de la instancia
		free(buffer);

		pthread_create(&hiloInstancia, NULL, (void*) hiloInstancia, instanciaRecibida);

	}else{
		pthread_mutex_lock(&mAuxiliarIdInstancia);
		auxiliarIdInstancia = *idInstancia;
		pthread_mutex_unlock(&mAuxiliarIdInstancia);
		instanciaRecibida = list_find(listaInstancias,(void *) buscarInstanciaPorId);

		instanciaRecibida->socket = socket;

		header header;
		header.protocolo = 4;

		cantEntradas_t cantEntradas = config_get_int_value(configuracion, "CantEntradas");
		tamEntradas_t tamEntradas = config_get_int_value(configuracion, "TamEntradas");

		t_list * listaDeClaves = list_filter(listaClaves,(void*) buscarClaves );

		int i;
		tamClave_t tamClave;
		cantClaves_t cantClaves = list_size(listaDeClaves);
		char * claveAux;

		int * buffer = malloc(sizeof(header) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) + sizeof(cantClaves_t));

		memcpy(buffer , &header.protocolo , sizeof(header) );
		memcpy(buffer+sizeof(header) , &cantEntradas , sizeof(cantEntradas_t) );
		memcpy(buffer+sizeof(header)+sizeof(cantEntradas_t) , &tamEntradas , sizeof(tamEntradas_t) );
		memcpy(buffer+sizeof(header)+sizeof(cantEntradas_t)+sizeof(tamEntradas_t) , &cantClaves , sizeof(cantClaves_t) );

		enviado = enviarBuffer (instanciaRecibida->socket , buffer , sizeof(header) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) + sizeof(cantClaves_t));

		while(enviado != 0){
			error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
			enviado = enviarBuffer (instanciaRecibida->socket , buffer ,sizeof(header) + sizeof(cantEntradas_t) + sizeof(tamEntradas_t) + sizeof(cantClaves_t));
		}

		free(buffer);

		for(i=0; i<cantClaves ;i++){

			claveAux = list_get(listaDeClaves, i);
			tamClave = string_length(claveAux);

			buffer = malloc(tamClave + sizeof(tamClave_t) );

			memcpy(buffer, &tamClave , sizeof(tamClave_t) );
			memcpy(buffer, claveAux, tamClave );

			enviado = enviarBuffer (instanciaRecibida->socket , buffer , tamClave + sizeof(tamClave_t) );

			while(enviado != 0){
				error_show ("no se envio correctamente los datos a la instancia, enviando nuevamente");
				enviado = enviarBuffer (instanciaRecibida->socket , buffer , tamClave + sizeof(tamClave_t) );
			}
			free(buffer);

			pthread_create(&hiloInstancia, NULL, (void*) hiloInstancia, instanciaRecibida);
		}

	}
}

int buscarClaves(char * clave){
	Instancia * instancia = dictionary_get( tablaDeClaves, clave);
	if(instancia->idinstancia ==  auxiliarIdInstancia)
		return 1;
	else
		return 0;

}

int buscarInstanciaPorId(Instancia * instancia){
	if(instancia->idinstancia == auxiliarIdInstancia)
		return 1;
	else
		return 0;
}

int buscarEsiPorId(Esi * esi){
	if(esi->idEsi == auxiliarIdEsi)
		return 1;
	else
		return 0;
}

bool compararPorEntradasLibres(Instancia instancia1,Instancia instancia2){
	return instancia1.entradasLibres >= instancia2.entradasLibres;
}

Instancia * algoritmoUsado(void){

	Instancia * instancia;
	if(strcmp("EL", Algoritmo) == 0){
		instancia = algoritmoEquitativeLoad();
	}else if(strcmp("LSU", Algoritmo) == 0){
		return algoritmoLastStatementUsed();
	}/*else if(strcmp("KE",*Algoritmo) == 0){
		return algoritmoKeyExplicit();
	} no voy a hacer estos algoritmos para este sabado*/

	return instancia;
}



Instancia * algoritmoLastStatementUsed(void){

	Instancia * instancia;
	t_list * listaAuxiliarLSU = list_duplicate(listaInstancias);
	pthread_mutex_lock(&mListaInst);

	if (hayQueSimular){
		hayQueSimular = 0;
	}

	list_sort(listaAuxiliarLSU, compararPorEntradasLibres);

	instancia = list_get(listaAuxiliarLSU, 0);

	return instancia;
	pthread_mutex_unlock(&mListaInst);
	//TODO falta el caso de empate en donde debo llamar a equitative load

	//TODO falta liberar la listaAuxiliarLSU

}

Instancia * algoritmoEquitativeLoad(void){

	Instancia * instancia;
	pthread_mutex_lock(&mListaInst);
	if (hayQueSimular){ // si hay que simular, hace lo mismo pero con una copia del contador, de modo que no afecta al algoritmo
		int copiaContador = contadorEquitativeLoad;
		do{
			if(copiaContador >= cantInstancias){
				copiaContador = 0;
				}else{
					copiaContador++;
				}

			instancia = list_get(listaInstancias, contadorEquitativeLoad);

		}while( seDesconectoSocket( instancia->socket) );
		hayQueSimular = 0;

	}else{
		do{
			if(contadorEquitativeLoad >= cantInstancias){
					contadorEquitativeLoad = 0;
				}else{
					contadorEquitativeLoad++;
				}

			instancia = list_get(listaInstancias, contadorEquitativeLoad);

		}while( seDesconectoSocket( instancia->socket) );
	}
	pthread_mutex_unlock(&mListaInst);

	return instancia;
}

int esperarYaceptar(socket_t socketCoordinador)
{
	unsigned int tam;
	struct sockaddr_in dir;
	listen(socketCoordinador , 5);
	int socket = accept(socketCoordinador, (void*)&dir, &tam);
	return socket;
	}

int validarPlanificador (socket_t socket) {
	header* handshake = NULL;
	int estadoDellegada;
	pthread_t hiloPlanificador;

	estadoDellegada = recibirMensaje(socket,sizeof(header),(void*) &handshake);
	if (estadoDellegada != 0)
	{
		error_show ("no se pudo recibir header de planificador.");
		return 1;
	}

	if (handshake->protocolo != 1)
	{
		close(socket);
		return 1;
	}
	else
	{
		pthread_create(&hiloPlanificador, NULL, (void*) hiloPlanificador, &socket);
		return 0;
	}
}

int max (int v1, int v2)
{
	if(v1 > v2)
		return v1;
	else
		return v2;
}

void liberarRecursos()
{
	// void list_destroy_and_destroy_elements(t_list*, void(*element_destroyer)(void*));
	// TODO liberar las listas

	close(socketCoordinador);
	log_destroy(logOperaciones);

	if(configuracion != NULL)
		config_destroy(configuracion);
	pthread_mutex_destroy (&mListaInst);

	pthread_mutex_destroy (&mAuxiliarIdInstancia);

	pthread_mutex_destroy (&mListaInst);

	pthread_mutex_destroy (&mEsiActual);
	pthread_cond_destroy (&sbEsiActual);

	pthread_mutex_destroy (&mInstanciaActual);
	pthread_cond_destroy (&sbInstanciaActual);

	pthread_mutex_destroy (&mRespuestaInstancia);
	pthread_cond_destroy (&sbRespuestaInstancia);

	pthread_cond_destroy (&sbRespuestaPlanificador);

	pthread_mutex_destroy (&mConsultarPorClave);
	pthread_cond_destroy (&sbConsultarPorClave);
}

void salirConError(char * error)
{
	error_show(error);
	liberarRecursos();
	salir_agraciadamente(1);

}

int crearConfiguracion(char* archivoConfig){
 if(!configuracion)
	configuracion = config_create(archivoConfig);
 return configuracion? 0 : 1;
}

 void inicializacion (char* dirConfig)
	{
	if(crearConfiguracion(dirConfig))
		salirConError("Fallo al leer el archivo de configuracion del planificador.\n ");
	Algoritmo = config_get_string_value(configuracion, "Algoritmo");

	socketConsultaClave = crearSocketServer(NULL, "0");

	listaEsi = list_create();
	listaInstancias = list_create();
	listaClaves = list_create();
	tablaDeClaves = dictionary_create();

	pthread_mutex_init (&mAuxiliarIdInstancia, NULL);

	pthread_mutex_init (&mListaInst, NULL);

	pthread_mutex_init(&mEsiActual,NULL);
	pthread_cond_init (&sbEsiActual, NULL);

	pthread_mutex_init(&mInstanciaActual,NULL);
	pthread_cond_init (&sbInstanciaActual, NULL);

	pthread_mutex_init (&mRespuestaInstancia, NULL);
	pthread_cond_init (&sbRespuestaInstancia, NULL);

	pthread_cond_init (&sbRespuestaPlanificador,NULL);

	pthread_mutex_init (&mConsultarPorClave,NULL);
	pthread_cond_init (&sbConsultarPorClave,NULL);

	logOperaciones = log_create("archivoLog.txt","logOperaciones",1,LOG_LEVEL_INFO);

}

