################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../pybindgen/examples/c/c.cc 

OBJS += \
./pybindgen/examples/c/c.o 

CC_DEPS += \
./pybindgen/examples/c/c.d 


# Each subdirectory must supply rules for building sources it contributes
pybindgen/examples/c/%.o: ../pybindgen/examples/c/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


