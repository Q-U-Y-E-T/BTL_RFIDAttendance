#ifndef RFID_DELAY_H
#define RFID_DELAY_H

#include "main.h"

#ifdef RFID_USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"

static inline void RFID_DelayMs(uint32_t milliseconds)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        TickType_t ticks = pdMS_TO_TICKS(milliseconds);
        vTaskDelay(ticks != 0U ? ticks : 1U);
    } else {
        HAL_Delay(milliseconds);
    }
}
#else
#define RFID_DelayMs HAL_Delay
#endif

#endif
