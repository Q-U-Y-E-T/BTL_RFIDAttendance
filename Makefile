######################################
# Project
######################################
TARGET = RFIDAttendance

######################################
# Build settings
######################################
DEBUG = 1
OPT = -Og

# 0: normal build
# 1: fake-card demo
DEMO ?= 0

ifneq ($(DEMO),0)
ifneq ($(DEMO),1)
$(error DEMO must be 0 or 1)
endif
endif

ifeq ($(DEMO),1)
BUILD_DIR = build-demo
else
BUILD_DIR = build
endif

######################################
# Core and HAL sources
######################################
C_SOURCES = \
Modules/RC522/rfid.c \
Core/Src/main.c \
Core/Src/stm32f1xx_it.c \
Core/Src/stm32f1xx_hal_msp.c \
Core/Src/system_stm32f1xx.c \
Core/Src/sysmem.c \
Core/Src/syscalls.c \
Core/Src/gpio.c \
Core/Src/spi.c \
Core/Src/i2c.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c

######################################
# FatFs and Storage sources
######################################
C_SOURCES += \
Middlewares/Third_Party/FatFs/src/ff.c \
Modules/Storage/sd_diskio.c \
Modules/Storage/storage.c

######################################
# Application sources
######################################
C_SOURCES += \
App/Src/app.c \
App/Src/storage_manager.c \
App/Src/feedback.c \
App/Src/attendance.c

######################################
# Optional demo
######################################
ifeq ($(DEMO),1)
C_SOURCES += Tests/attendance_demo.c
endif

######################################
# Assembly sources
######################################
ASM_SOURCES = \
startup_stm32f103xb.s

ASMM_SOURCES =

######################################
# Toolchain
######################################
PREFIX = arm-none-eabi-

ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif

HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

######################################
# MCU
######################################
CPU = -mcpu=cortex-m3
FPU =
FLOAT-ABI =
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

######################################
# Preprocessor definitions
######################################
AS_DEFS =

C_DEFS = \
-DUSE_HAL_DRIVER \
-DSTM32F103xB \
-DAPP_ENABLE_DEMO=$(DEMO)

######################################
# Header paths
######################################
AS_INCLUDES =

C_INCLUDES = \
-ICore/Inc \
-IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
-IDrivers/STM32F1xx_HAL_Driver/Inc \
-IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
-IDrivers/CMSIS/Include \
-IApp/Inc \
-IModules/RC522 \
-IModules/LCD \
-IModules/Storage \
-IMiddlewares/Third_Party/FatFs/src

ifeq ($(DEMO),1)
C_INCLUDES += -ITests
endif

######################################
# Compiler flags
######################################
include App/rfid_task_queue.mk

ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) \
-Wall -fdata-sections -ffunction-sections

CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) \
-Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG),1)
CFLAGS += -g -gdwarf-2
ASFLAGS += -g -gdwarf-2
endif

CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

######################################
# Linker
######################################
LDSCRIPT = STM32F103xx_FLASH.ld

LIBS = -lc -lm -lnosys
LIBDIR =

LDFLAGS = $(MCU) -specs=nano.specs \
-T$(LDSCRIPT) $(LIBDIR) $(LIBS) \
-Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
-Wl,--gc-sections

######################################
# Default target
######################################
.DEFAULT_GOAL := all
.PHONY: all clean clean-all test-host

all: $(BUILD_DIR)/$(TARGET).elf \
     $(BUILD_DIR)/$(TARGET).hex \
     $(BUILD_DIR)/$(TARGET).bin

######################################
# Object files
######################################
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASMM_SOURCES:.S=.o)))
vpath %.S $(sort $(dir $(ASMM_SOURCES)))

######################################
# Compile
######################################
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $(C_DEFS) $(C_INCLUDES) $< -o $@

$(BUILD_DIR)/%.o: %.S Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $(C_DEFS) $(C_INCLUDES) $< -o $@

######################################
# Link and generate firmware
######################################
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir -p $@

######################################
# Clean
######################################
clean:
	$(RM) -r $(BUILD_DIR)

clean-all:
	$(RM) -r build build-demo build-host

# Desktop tests: no ARM toolchain or STM32 required.
test-host:
	python3 Tests/run_host_tests.py

######################################
# Dependencies
######################################
-include $(wildcard $(BUILD_DIR)/*.d)
