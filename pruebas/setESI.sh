#!/bin/bash

[ ! -d pruebas/esi ] && git clone https://github.com/sisoputnfrba/Pruebas-ESI.git pruebas/esi || echo "Los ESI ya están clonados"
read -n 1 -s
make -C biblioCompartida/make
read -n 1 -s
make -C esi/make
read -n 1 -s
nano "pruebas/configuraciones/esi.config"
