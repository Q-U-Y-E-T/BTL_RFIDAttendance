#ifndef TASK_RFID_READ_H
#define TASK_RFID_READ_H

#include "FreeRTOS.h"
#include "task.h"
#include "rfid.h"
#include "card_cooldown.h"

#define RFID_TASK_STACK_WORDS 384U
#define RFID_TASK_PRIORITY 3U
#define RFID_TASK_PERIOD_MS 50U

typedef struct {
    uint32_t init_errors;
    uint32_t read_errors;
    uint32_t cards_read;
    uint32_t cards_queued;
    uint32_t cards_suppressed;
    uint32_t cooldown_full;
    uint32_t queue_waits;
    uint32_t stack_free_words;
    RFID_Status_t init_status;
    RFID_Status_t last_status;
    CardEvent_t last_card;
} RFID_TaskStats_t;

extern volatile RFID_TaskStats_t g_rfid_task_stats;

BaseType_t RFID_Task_Start(void);

#endif
