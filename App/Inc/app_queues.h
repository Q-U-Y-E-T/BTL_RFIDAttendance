#ifndef APP_QUEUES_H
#define APP_QUEUES_H
#include "FreeRTOS.h"
#include "queue.h"
#include "app_types.h"
#include "reader_event.h"
#define APP_CARD_QUEUE_LENGTH 8U
#define APP_DISPLAY_QUEUE_LENGTH 4U
extern QueueHandle_t g_reader_event_queue;
extern QueueHandle_t g_display_event_queue;

extern QueueHandle_t g_system_display_queue;
BaseType_t App_Queues_Init(void);
#endif
