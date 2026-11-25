TARGET := RFIDAttendance
BUILD_DIR := build-rtos
DEBUG ?= 1
OPT ?= -Og
LIST_FILE ?= Tools/Provision/DanhSach.csv
GENERATED_LIST := $(BUILD_DIR)/generated/embedded_list_data.h

ifneq ($(origin PROVISION),undefined)
$(error Khong con firmware rieng PROVISION. Sua DanhSach.csv roi chay make)
endif
ifneq ($(origin REPLACE_LIST),undefined)
$(error Khong can REPLACE_LIST. Firmware tu cap nhat va sao luu danh sach)
endif

ifneq ($(origin DEMO),undefined)
$(error DEMO da duoc bo. Ban nay chi chay he thong RTOS: make)
endif
ifneq ($(origin RFID_RTOS_TEST),undefined)
$(error RFID_RTOS_TEST da duoc bo. Ban nay chi chay he thong RTOS: make)
endif
ifneq ($(origin RTOS),undefined)
$(error Khong con lua chon RTOS. Ban nay luon chay FreeRTOS: make)
endif

TOOLCHAIN_PREFIX := $(if $(GCC_PATH),$(GCC_PATH)/,)arm-none-eabi-
CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
SIZE := $(TOOLCHAIN_PREFIX)size

HAL := Drivers/STM32F1xx_HAL_Driver
FREERTOS := Middlewares/Third_Party/FreeRTOS/Source
FATFS := Middlewares/Third_Party/FatFs/src

C_SOURCES := \
Core/Src/main.c \
Core/Src/stm32f1xx_it.c \
Core/Src/stm32f1xx_hal_msp.c \
Core/Src/system_stm32f1xx.c \
Core/Src/sysmem.c \
Core/Src/syscalls.c \
Core/Src/gpio.c \
Core/Src/spi.c \
Core/Src/i2c.c \
App/Src/app.c \
App/Src/app_input.c \
App/Src/app_rtos.c \
App/Src/app_queues.c \
App/Src/task_rfid_read.c \
App/Src/storage_manager.c \
App/Src/attendance.c \
App/Src/feedback.c \
Modules/RC522/rfid.c \
Modules/LCD/lcd.c \
Modules/Storage/sd_diskio.c \
Modules/Storage/storage.c \
$(FATFS)/ff.c \
$(FREERTOS)/tasks.c \
$(FREERTOS)/queue.c \
$(FREERTOS)/list.c \
$(FREERTOS)/portable/GCC/ARM_CM3/port.c \
$(FREERTOS)/portable/MemMang/heap_4.c \
$(HAL)/Src/stm32f1xx_hal.c \
$(HAL)/Src/stm32f1xx_hal_gpio.c \
$(HAL)/Src/stm32f1xx_hal_gpio_ex.c \
$(HAL)/Src/stm32f1xx_hal_rcc.c \
$(HAL)/Src/stm32f1xx_hal_rcc_ex.c \
$(HAL)/Src/stm32f1xx_hal_dma.c \
$(HAL)/Src/stm32f1xx_hal_cortex.c \
$(HAL)/Src/stm32f1xx_hal_pwr.c \
$(HAL)/Src/stm32f1xx_hal_flash.c \
$(HAL)/Src/stm32f1xx_hal_flash_ex.c \
$(HAL)/Src/stm32f1xx_hal_exti.c \
$(HAL)/Src/stm32f1xx_hal_spi.c \
$(HAL)/Src/stm32f1xx_hal_i2c.c

ASM_SOURCES := startup_stm32f103xb.s
MCU := -mcpu=cortex-m3 -mthumb
C_DEFS := -DUSE_HAL_DRIVER -DSTM32F103xB
C_INCLUDES := \
-ICore/Inc \
-IApp/Inc \
-IModules/RC522 \
-IModules/LCD \
-IModules/Storage \
-I$(FATFS) \
-I$(HAL)/Inc \
-I$(HAL)/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
-IDrivers/CMSIS/Include \
-I$(FREERTOS)/include \
-I$(FREERTOS)/portable/GCC/ARM_CM3

C_SOURCES += App/Src/embedded_list.c Modules/Storage/student_list_update.c
C_INCLUDES += -I$(BUILD_DIR)/generated

CFLAGS := $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -std=gnu11 \
-Wall -fdata-sections -ffunction-sections
ASFLAGS := $(MCU) $(C_DEFS) $(C_INCLUDES)
ifeq ($(DEBUG),1)
CFLAGS += -g -gdwarf-2
ASFLAGS += -g -gdwarf-2
endif

LDSCRIPT := STM32F103xx_FLASH.ld
LDFLAGS := $(MCU) -specs=nano.specs -T$(LDSCRIPT) \
-Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
LIBS := -lc -lm -lnosys

OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(ASM_SOURCES:.s=.o))

.DEFAULT_GOAL := all
.PHONY: all clean clean-all test-host test-list-update check-source check test-make-data force-list
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

force-list:

$(GENERATED_LIST): $(LIST_FILE) Tools/Provision/generate_list.py Makefile force-list
	mkdir -p $(@D)
	python3 Tools/Provision/generate_list.py "$(LIST_FILE)" "$@"

$(OBJECTS): $(GENERATED_LIST)

$(BUILD_DIR)/%.o: %.c Makefile
	mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -MMD -MP -MF $(@:.o=.d) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile
	mkdir -p $(@D)
	$(CC) -x assembler-with-cpp -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

clean:
	$(RM) -r $(BUILD_DIR)

clean-all:
	$(RM) -r build-rtos build-host build build-demo build_rfid_rtos build-provision build-provision-replace

test-host:
	python3 Tests/run_host_tests.py

check-source:
	python3 Tests/check_source.py

test-list-update:
	python3 Tests/test_list_update.py

test-make-data:
	python3 Tests/test_make_data.py

check: test-host check-source test-list-update test-make-data

-include $(OBJECTS:.o=.d)

.PHONY: flash
flash: all
	st-flash write $(BUILD_DIR)/$(TARGET).bin 0x08000000
