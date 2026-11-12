#ifndef APP_QUEUES_H
#define APP_QUEUES_H

#include "FreeRTOS.h"
#include "queue.h"
#include "app_types.h"

#define APP_CARD_QUEUE_LENGTH 8U

/* Created once before task creation. The queue copies complete CardEvent_t values. */
extern QueueHandle_t g_card_event_queue;
BaseType_t App_Queues_Init(void);

#endif
