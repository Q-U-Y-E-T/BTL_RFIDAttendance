#include "app_queues.h"

QueueHandle_t g_card_event_queue;

BaseType_t App_Queues_Init(void)
{
    if (g_card_event_queue == NULL) {
        g_card_event_queue = xQueueCreate(APP_CARD_QUEUE_LENGTH, sizeof(CardEvent_t));
    }
    return g_card_event_queue != NULL ? pdPASS : pdFAIL;
}
