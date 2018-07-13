#!/bin/bash

make -C biblioCompartida/make
read -n 1 -s
make -C esi/make
read -n 1 -s
nano "pruebas/configuraciones/esi.config"
