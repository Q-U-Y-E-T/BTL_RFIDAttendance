#include "app_queues.h"
QueueHandle_t g_reader_event_queue;
QueueHandle_t g_display_event_queue;
QueueHandle_t g_system_display_queue;
BaseType_t App_Queues_Init(void)
{
    if (!g_reader_event_queue)
        g_reader_event_queue = xQueueCreate(APP_CARD_QUEUE_LENGTH, sizeof(ReaderEvent_t));
    if (!g_display_event_queue)
        g_display_event_queue = xQueueCreate(APP_DISPLAY_QUEUE_LENGTH, sizeof(DisplayEvent_t));
    if (!g_system_display_queue)
        g_system_display_queue = xQueueCreate(1U, sizeof(DisplayEvent_t));
    return g_reader_event_queue && g_display_event_queue && g_system_display_queue
           ? pdPASS : pdFAIL;
}
