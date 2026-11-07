################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adc.c \
../Core/Src/aft.c \
../Core/Src/card.c \
../Core/Src/ds2401.c \
../Core/Src/flash.c \
../Core/Src/hello.c \
../Core/Src/lcd.c \
../Core/Src/longpoll.c \
../Core/Src/main.c \
../Core/Src/mgmt.c \
../Core/Src/sas.c \
../Core/Src/sniffer.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/tito.c \
../Core/Src/uart.c \
../Core/Src/udp_echo.c \
../Core/Src/udp_server.c \
../Core/Src/watchdog.c 

OBJS += \
./Core/Src/adc.o \
./Core/Src/aft.o \
./Core/Src/card.o \
./Core/Src/ds2401.o \
./Core/Src/flash.o \
./Core/Src/hello.o \
./Core/Src/lcd.o \
./Core/Src/longpoll.o \
./Core/Src/main.o \
./Core/Src/mgmt.o \
./Core/Src/sas.o \
./Core/Src/sniffer.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/tito.o \
./Core/Src/uart.o \
./Core/Src/udp_echo.o \
./Core/Src/udp_server.o \
./Core/Src/watchdog.o 

C_DEPS += \
./Core/Src/adc.d \
./Core/Src/aft.d \
./Core/Src/card.d \
./Core/Src/ds2401.d \
./Core/Src/flash.d \
./Core/Src/hello.d \
./Core/Src/lcd.d \
./Core/Src/longpoll.d \
./Core/Src/main.d \
./Core/Src/mgmt.d \
./Core/Src/sas.d \
./Core/Src/sniffer.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/tito.d \
./Core/Src/uart.d \
./Core/Src/udp_echo.d \
./Core/Src/udp_server.d \
./Core/Src/watchdog.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F427xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -I../USB_HOST/App -I../USB_HOST/Target -I../Middlewares/Third_Party/FatFs/src -I../Middlewares/ST/STM32_USB_Host_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/BSP/Components/dp83848 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adc.cyclo ./Core/Src/adc.d ./Core/Src/adc.o ./Core/Src/adc.su ./Core/Src/aft.cyclo ./Core/Src/aft.d ./Core/Src/aft.o ./Core/Src/aft.su ./Core/Src/card.cyclo ./Core/Src/card.d ./Core/Src/card.o ./Core/Src/card.su ./Core/Src/ds2401.cyclo ./Core/Src/ds2401.d ./Core/Src/ds2401.o ./Core/Src/ds2401.su ./Core/Src/flash.cyclo ./Core/Src/flash.d ./Core/Src/flash.o ./Core/Src/flash.su ./Core/Src/hello.cyclo ./Core/Src/hello.d ./Core/Src/hello.o ./Core/Src/hello.su ./Core/Src/lcd.cyclo ./Core/Src/lcd.d ./Core/Src/lcd.o ./Core/Src/lcd.su ./Core/Src/longpoll.cyclo ./Core/Src/longpoll.d ./Core/Src/longpoll.o ./Core/Src/longpoll.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/mgmt.cyclo ./Core/Src/mgmt.d ./Core/Src/mgmt.o ./Core/Src/mgmt.su ./Core/Src/sas.cyclo ./Core/Src/sas.d ./Core/Src/sas.o ./Core/Src/sas.su ./Core/Src/sniffer.cyclo ./Core/Src/sniffer.d ./Core/Src/sniffer.o ./Core/Src/sniffer.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/tito.cyclo ./Core/Src/tito.d ./Core/Src/tito.o ./Core/Src/tito.su ./Core/Src/uart.cyclo ./Core/Src/uart.d ./Core/Src/uart.o ./Core/Src/uart.su ./Core/Src/udp_echo.cyclo ./Core/Src/udp_echo.d ./Core/Src/udp_echo.o ./Core/Src/udp_echo.su ./Core/Src/udp_server.cyclo ./Core/Src/udp_server.d ./Core/Src/udp_server.o ./Core/Src/udp_server.su ./Core/Src/watchdog.cyclo ./Core/Src/watchdog.d ./Core/Src/watchdog.o ./Core/Src/watchdog.su

.PHONY: clean-Core-2f-Src

