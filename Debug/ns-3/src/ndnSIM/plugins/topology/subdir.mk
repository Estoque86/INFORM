################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../ns-3/src/ndnSIM/plugins/topology/annotated-topology-reader.cc \
../ns-3/src/ndnSIM/plugins/topology/rocketfuel-map-reader.cc \
../ns-3/src/ndnSIM/plugins/topology/rocketfuel-maps-cch-to-annotaded.cc \
../ns-3/src/ndnSIM/plugins/topology/rocketfuel-weights-reader.cc 

OBJS += \
./ns-3/src/ndnSIM/plugins/topology/annotated-topology-reader.o \
./ns-3/src/ndnSIM/plugins/topology/rocketfuel-map-reader.o \
./ns-3/src/ndnSIM/plugins/topology/rocketfuel-maps-cch-to-annotaded.o \
./ns-3/src/ndnSIM/plugins/topology/rocketfuel-weights-reader.o 

CC_DEPS += \
./ns-3/src/ndnSIM/plugins/topology/annotated-topology-reader.d \
./ns-3/src/ndnSIM/plugins/topology/rocketfuel-map-reader.d \
./ns-3/src/ndnSIM/plugins/topology/rocketfuel-maps-cch-to-annotaded.d \
./ns-3/src/ndnSIM/plugins/topology/rocketfuel-weights-reader.d 


# Each subdirectory must supply rules for building sources it contributes
ns-3/src/ndnSIM/plugins/topology/%.o: ../ns-3/src/ndnSIM/plugins/topology/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


