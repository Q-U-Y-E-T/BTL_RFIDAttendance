#include "task_rfid_read.h"
#include "app_queues.h"
#include <string.h>

volatile RFID_TaskStats_t g_rfid_task_stats = {.init_status = RFID_ERROR};
static TaskHandle_t rfid_task_handle;

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len; /* Zero means this slot has not been used. */
    TickType_t last_sent;
} RFID_CooldownEntry_t;

static RFID_CooldownEntry_t cooldown_entries[RFID_CARD_COOLDOWN_SLOTS];

enum {
    COOLDOWN_SUPPRESSED = -1,
    COOLDOWN_FULL = -2,
    COOLDOWN_INVALID_UID = -3
};

/* Only Task_RFID_Read owns this table. Never evict an unexpired UID. */
static int Cooldown_FindSlot(const CardEvent_t *card, TickType_t now,
                             TickType_t interval)
{
    int reusable = COOLDOWN_FULL;

    if (card->uid_len != 4U && card->uid_len != 7U && card->uid_len != 10U) {
        return COOLDOWN_INVALID_UID;
    }
    for (unsigned i = 0; i < RFID_CARD_COOLDOWN_SLOTS; ++i) {
        const RFID_CooldownEntry_t *entry = &cooldown_entries[i];
        TickType_t elapsed = (TickType_t)(now - entry->last_sent);

        if (entry->uid_len == card->uid_len
            && memcmp(entry->uid, card->uid, card->uid_len) == 0) {
            return elapsed < interval ? COOLDOWN_SUPPRESSED : (int)i;
        }
        if (reusable == COOLDOWN_FULL
            && (entry->uid_len == 0U || elapsed >= interval)) {
            reusable = (int)i;
        }
    }
    return reusable;
}

static void Cooldown_Record(unsigned slot, const CardEvent_t *card,
                             TickType_t now)
{
    RFID_CooldownEntry_t *entry = &cooldown_entries[slot];
    memcpy(entry->uid, card->uid, card->uid_len);
    entry->uid_len = card->uid_len;
    entry->last_sent = now;
}

static void Task_RFID_Read(void *argument)
{
    CardEvent_t card;
    TickType_t last_wake;
    const TickType_t period = pdMS_TO_TICKS(RFID_TASK_PERIOD_MS);
    const TickType_t cooldown = pdMS_TO_TICKS(RFID_CARD_COOLDOWN_MS);
    (void)argument;
    configASSERT(period > 0U);
    configASSERT(sizeof(TickType_t) >= sizeof(uint32_t));
    configASSERT(RFID_CARD_COOLDOWN_MS == 0UL || cooldown > 0U);

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
            int slot = Cooldown_FindSlot(&card, xTaskGetTickCount(), cooldown);
            if (slot >= 0) {
                /* Keep the detected event until the consumer makes room. */
                while (xQueueSend(g_card_event_queue, &card,
                                  pdMS_TO_TICKS(100U)) != pdPASS) {
                    ++g_rfid_task_stats.queue_waits;
                }
                ++g_rfid_task_stats.cards_queued;
                /* Start the interval only after the queue accepts the event. */
                Cooldown_Record((unsigned)slot, &card, xTaskGetTickCount());
            } else if (slot == COOLDOWN_SUPPRESSED) {
                ++g_rfid_task_stats.cards_suppressed;
            } else if (slot == COOLDOWN_FULL) {
                ++g_rfid_task_stats.cooldown_full;
            } else {
                ++g_rfid_task_stats.read_errors;
            }
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
