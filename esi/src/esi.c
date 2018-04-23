#include <stdio.h>
#include <stdlib.h>
#include <parsi/parser.h>

int main(int argc, char **argv) {
	t_esi_operacion prueba = parse("get prueba");
	printf("%s", prueba.argumentos.GET.clave);
	return EXIT_SUCCESS;
}
