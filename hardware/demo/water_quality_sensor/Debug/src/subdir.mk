################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hal_entry.c \
../src/hal_warmstart.c 

C_DEPS += \
./src/hal_entry.d \
./src/hal_warmstart.d 

CREF += \
water_quality_sensor.cref 

OBJS += \
./src/hal_entry.o \
./src/hal_warmstart.o 

MAP += \
water_quality_sensor.map 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mlittle-endian -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -ffunction-sections -fdata-sections -fno-strict-aliasing -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -g3 -std=c99 -fshort-enums -fno-unroll-loops -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\ra_gen" -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\device\\inc" -I"." -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\ra_cfg\\fsp_cfg\\bsp" -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\ra_cfg\\fsp_cfg" -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\src" -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\ra\\fsp\\inc" -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\ra\\fsp\\inc\\api" -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\ra\\fsp\\inc\\instances" -I"G:\\AquaGarden\\hardware\\demo\\water_quality_sensor\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

