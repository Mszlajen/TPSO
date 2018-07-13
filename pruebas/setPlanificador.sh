#!/bin/bash

make -C biblioCompartida/make
read -n 1 -s
make -C planificador/make
read -n 1 -s
nano "pruebas/configuraciones/minima/planificador.config"
nano "pruebas/configuraciones/deadlock/planificador.config"
nano "pruebas/configuraciones/distribucion/planificador.config"
nano "pruebas/configuraciones/estres/planificador.config"
nano "pruebas/configuraciones/planificacion/planificador.config"
nano "pruebas/configuraciones/reemplazo/planificador.config"