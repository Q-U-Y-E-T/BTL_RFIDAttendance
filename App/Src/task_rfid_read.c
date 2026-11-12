#include "task_rfid_read.h"
#include "app_queues.h"

volatile RFID_TaskStats_t g_rfid_task_stats = {.init_status = RFID_ERROR};
static TaskHandle_t rfid_task_handle;

static void Task_RFID_Read(void *argument)
{
    CardEvent_t card;
    TickType_t last_wake;
    const TickType_t period = pdMS_TO_TICKS(RFID_TASK_PERIOD_MS);
    (void)argument;
    configASSERT(period > 0U);

    do {
        g_rfid_task_stats.init_status = RFID_Init();
        if (g_rfid_task_stats.init_status != RFID_OK) {
            ++g_rfid_task_stats.init_errors;
            vTaskDelay(pdMS_TO_TICKS(500U));
        }
    } while (g_rfid_task_stats.init_status != RFID_OK);

    last_wake = xTaskGetTickCount();
    for (;;) {
        RFID_Status_t status = RFID_ReadCard(&card);
        if (status == RFID_OK) {
            ++g_rfid_task_stats.cards_read;
            /* Keep the detected event until the consumer makes room. */
            while (xQueueSend(g_card_event_queue, &card,
                              pdMS_TO_TICKS(100U)) != pdPASS) {
                ++g_rfid_task_stats.queue_waits;
            }
            ++g_rfid_task_stats.cards_queued;
        } else if (status == RFID_ERROR) {
            ++g_rfid_task_stats.read_errors;
        }
        g_rfid_task_stats.stack_free_words = uxTaskGetStackHighWaterMark(NULL);

        /* A long SD/consumer stall must not cause a burst of catch-up scans. */
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_wake) >= period) {
            last_wake = now;
        }
        vTaskDelayUntil(&last_wake, period);
    }
}

BaseType_t RFID_Task_Start(void)
{
    if (g_card_event_queue == NULL || rfid_task_handle != NULL) {
        return pdFAIL;
    }
    return xTaskCreate(Task_RFID_Read, "RFID_Read", RFID_TASK_STACK_WORDS,
                       NULL, RFID_TASK_PRIORITY, &rfid_task_handle);
}
