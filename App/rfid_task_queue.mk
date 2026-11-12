# Input task and queue test profile. Included before the main Makefile flags.
RFID_RTOS_TEST ?= 0
RFID_QUEUE_TEST_PAUSE_MS ?= 0

ifeq ($(RFID_RTOS_TEST),1)
BUILD_DIR := build_rfid_rtos
FREERTOS_SOURCE := Middlewares/Third_Party/FreeRTOS/Source
C_DEFS += -DRFID_USE_FREERTOS
C_DEFS += -DRFID_QUEUE_TEST_PAUSE_MS=$(RFID_QUEUE_TEST_PAUSE_MS)
C_INCLUDES += -ITests
C_INCLUDES += -I$(FREERTOS_SOURCE)/include
C_INCLUDES += -I$(FREERTOS_SOURCE)/portable/GCC/ARM_CM3

C_SOURCES := $(filter-out Core/Src/main.c Core/Src/stm32f1xx_it.c,$(C_SOURCES))
C_SOURCES += App/Src/app_queues.c
C_SOURCES += App/Src/task_rfid_read.c
C_SOURCES += Tests/main_rfid_queue_test.c
C_SOURCES += Tests/freertos_test_interrupts.c
C_SOURCES += $(FREERTOS_SOURCE)/tasks.c
C_SOURCES += $(FREERTOS_SOURCE)/queue.c
C_SOURCES += $(FREERTOS_SOURCE)/list.c
C_SOURCES += $(FREERTOS_SOURCE)/portable/GCC/ARM_CM3/port.c
C_SOURCES += $(FREERTOS_SOURCE)/portable/MemMang/heap_4.c
endif
