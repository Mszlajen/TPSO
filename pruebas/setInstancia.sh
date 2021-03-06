#!/bin/bash

[ ! -d /home/utnso/inst1 ] && mkdir -p /home/utnso/inst1
[ ! -d /home/utnso/inst2 ] && mkdir -p /home/utnso/inst2
[ ! -d /home/utnso/inst3 ] && mkdir -p /home/utnso/inst3
make -C biblioCompartida/make
read -n 1 -s
make -C instancia/make
read -n 1 -s
nano "pruebas/configuraciones/minima/instancia1.config"
nano "pruebas/configuraciones/minima/instancia2.config"
nano "pruebas/configuraciones/minima/instancia3.config"
nano "pruebas/configuraciones/deadlock/instancia1.config"
nano "pruebas/configuraciones/deadlock/instancia2.config"
nano "pruebas/configuraciones/distribucion/instancia1.config"
nano "pruebas/configuraciones/distribucion/instancia2.config"
nano "pruebas/configuraciones/estres/instancia1.config"
nano "pruebas/configuraciones/estres/instancia2.config"
nano "pruebas/configuraciones/planificacion/instancia1.config"
nano "pruebas/configuraciones/planificacion/instancia2.config"
nano "pruebas/configuraciones/reemplazo/instancia1.config"
nano "pruebas/configuraciones/reemplazo/instancia2.config"

