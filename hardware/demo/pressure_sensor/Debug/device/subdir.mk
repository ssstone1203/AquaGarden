################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../device/dev_ps_driver.c 

C_DEPS += \
./device/dev_ps_driver.d 

CREF += \
pressure_sensor.cref 

OBJS += \
./device/dev_ps_driver.o 

MAP += \
pressure_sensor.map 


# Each subdirectory must supply rules for building sources it contributes
device/%.o: ../device/%.c
	@echo 'Building file: $<'
	$(file > $@.in,-mcpu=cortex-m85 -mthumb -mlittle-endian -mfloat-abi=hard -O2 -ffunction-sections -fdata-sections -fno-strict-aliasing -fmessage-length=0 -funsigned-char -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Waggregate-return -Wno-parentheses-equality -Wfloat-equal -g3 -std=c99 -flax-vector-conversions -fshort-enums -fno-unroll-loops -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra_gen" -I"." -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra_cfg\\fsp_cfg\\bsp" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra_cfg\\fsp_cfg" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\src" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\device" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\fsp\\inc" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\fsp\\inc\\api" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\fsp\\inc\\instances" -I"G:\\AquaGarden\\hardware\\demo\\pressure_sensor\\ra\\arm\\CMSIS_6\\CMSIS\\Core\\Include" -D_RENESAS_RA_ -D_RA_CORE=CM85 -D_RA_ORDINAL=1 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -x c "$<" -c -o "$@")
	@clang --target=arm-none-eabi @"$@.in"

