################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/adminESI.c \
../src/colas.c \
../src/configuracion.c \
../src/planificador.c 

OBJS += \
./src/adminESI.o \
./src/colas.o \
./src/configuracion.o \
./src/planificador.o 

C_DEPS += \
./src/adminESI.d \
./src/colas.d \
./src/configuracion.d \
./src/planificador.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"../../biblioCompartida" -O0 -g3 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


