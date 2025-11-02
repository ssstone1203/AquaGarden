################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra_gen/common_data.c \
../ra_gen/hal_data.c \
../ra_gen/main.c \
../ra_gen/pin_data.c \
../ra_gen/vector_data.c 

C_DEPS += \
./ra_gen/common_data.d \
./ra_gen/hal_data.d \
./ra_gen/main.d \
./ra_gen/pin_data.d \
./ra_gen/vector_data.d 

CREF += \
pressure_sensor.cref 

OBJS += \
./ra_gen/common_data.o \
./ra_gen/hal_data.o \
./ra_gen/main.o \
./ra_gen/pin_data.o \
./ra_gen/vector_data.o 

MAP += \
pressure_sensor.map 


# Each subdirectory must supply rules for building sources it contributes
ra_gen/%.o: ../ra_gen/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m85 -mthumb -mlittle-endian -mfloat-abi=hard -O2 -ffunction-sections -fdata-sections -fno-strict-aliasing -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -g3 -std=c99 -flax-vector-conversions -fshort-enums -fno-unroll-loops -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra_gen" -I"." -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra_cfg\\fsp_cfg\\bsp" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra_cfg\\fsp_cfg" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\src" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\device" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\fsp\\inc" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\fsp\\inc\\api" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\fsp\\inc\\instances" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -D_RENESAS_RA_ -D_RA_CORE=CM85 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

