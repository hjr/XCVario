################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../onewire_rmt.c 

CPP_SRCS += \
../BME280_ESP32_SPI.cpp \
../BMPVario.cpp \
../BTSender.cpp \
../BatVoltage.cpp \
../DS18B20.cpp \
../DallasRmt.cpp \
../DotDisplay.cpp \
../ESP32NVS.cpp \
../ESPAudio.cpp \
../ESPRotary.cpp \
../I2C.cpp \
../MP5004DP.cpp \
../OnewireRmt.cpp \
../OpenVario.cpp \
../PWMOut.cpp \
../Polars.cpp \
../S2F.cpp \
../SPI.cpp \
../Setup.cpp \
../SetupMenu.cpp \
../SetupVolt.cpp \
../Switch.cpp \
../Version.cpp \
../mcp3221.cpp \
../sensor.cpp \
../u8g2_esp32_hal.cpp 

OBJS += \
./BME280_ESP32_SPI.o \
./BMPVario.o \
./BTSender.o \
./BatVoltage.o \
./DS18B20.o \
./DallasRmt.o \
./DotDisplay.o \
./ESP32NVS.o \
./ESPAudio.o \
./ESPRotary.o \
./I2C.o \
./MP5004DP.o \
./OnewireRmt.o \
./OpenVario.o \
./PWMOut.o \
./Polars.o \
./S2F.o \
./SPI.o \
./Setup.o \
./SetupMenu.o \
./SetupVolt.o \
./Switch.o \
./Version.o \
./mcp3221.o \
./onewire_rmt.o \
./sensor.o \
./u8g2_esp32_hal.o 

C_DEPS += \
./onewire_rmt.d 

CPP_DEPS += \
./BME280_ESP32_SPI.d \
./BMPVario.d \
./BTSender.d \
./BatVoltage.d \
./DS18B20.d \
./DallasRmt.d \
./DotDisplay.d \
./ESP32NVS.d \
./ESPAudio.d \
./ESPRotary.d \
./I2C.d \
./MP5004DP.d \
./OnewireRmt.d \
./OpenVario.d \
./PWMOut.d \
./Polars.d \
./S2F.d \
./SPI.d \
./Setup.d \
./SetupMenu.d \
./SetupVolt.d \
./Switch.d \
./Version.d \
./mcp3221.d \
./sensor.d \
./u8g2_esp32_hal.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


