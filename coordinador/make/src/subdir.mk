################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/coordinador.c 

OBJS += \
./src/coordinador.o 

C_DEPS += \
./src/coordinador.d 


# Each subdirectory must supply rules for building sources it contributes
src/coordinador.o: ../src/coordinador.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"../../biblioCompartida" -O0 -g3 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"src/coordinador.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


