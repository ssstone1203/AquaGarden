################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/r_sci_i2c/r_sci_i2c.c 

C_DEPS += \
./ra/fsp/src/r_sci_i2c/r_sci_i2c.d 

CREF += \
th_sensor.cref 

OBJS += \
./ra/fsp/src/r_sci_i2c/r_sci_i2c.o 

MAP += \
th_sensor.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/r_sci_i2c/%.o: ../ra/fsp/src/r_sci_i2c/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mlittle-endian -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -ffunction-sections -fdata-sections -fno-strict-aliasing -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -g3 -std=c99 -fshort-enums -fno-unroll-loops -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\ra_gen" -I"." -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\ra_cfg\\fsp_cfg\\bsp" -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\device\\inc" -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\ra_cfg\\fsp_cfg" -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\src" -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\ra\\fsp\\inc" -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\ra\\fsp\\inc\\api" -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\ra\\fsp\\inc\\instances" -I"E:\\AquaGarden\\hardware\\demo\\th_sensor\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

