################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/Colas/bloqueados.c \
../src/Colas/ejecutando.c \
../src/Colas/finalizados.c \
../src/Colas/ready.c 

OBJS += \
./src/Colas/bloqueados.o \
./src/Colas/ejecutando.o \
./src/Colas/finalizados.o \
./src/Colas/ready.o 

C_DEPS += \
./src/Colas/bloqueados.d \
./src/Colas/ejecutando.d \
./src/Colas/finalizados.d \
./src/Colas/ready.d 


# Each subdirectory must supply rules for building sources it contributes
src/Colas/%.o: ../src/Colas/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"../../../../biblioCompartida" -O0 -g3 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


