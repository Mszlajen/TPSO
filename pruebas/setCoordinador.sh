#!/bin/bash

make -C biblioCompartida/make
read -n 1 -s
make -C coordinador/make
read -n 1 -s
nano "pruebas/configuraciones/minima/coordinador.config"
nano "pruebas/configuraciones/deadlock/coordinador.config"
nano "pruebas/configuraciones/distribucion/coordinador.config"
nano "pruebas/configuraciones/estres/coordinador.config"
nano "pruebas/configuraciones/planificacion/coordinador.config"
nano "pruebas/configuraciones/reemplazo/coordinador.config"
