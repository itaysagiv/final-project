################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../CacheFuncs.c \
../EvictionSet.c \
../Probe.c \
../main.c 

O_SRCS += \
../CacheFuncs.o 

OBJS += \
./CacheFuncs.o \
./EvictionSet.o \
./Probe.o \
./main.o 

C_DEPS += \
./CacheFuncs.d \
./EvictionSet.d \
./Probe.d \
./main.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


