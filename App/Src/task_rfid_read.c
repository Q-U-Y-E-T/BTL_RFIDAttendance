#include "task_rfid_read.h"
#include "app_queues.h"
#include "main.h"
#include <string.h>

volatile RFID_TaskStats_t g_rfid_task_stats = {.init_status = RFID_ERROR};
static TaskHandle_t rfid_task_handle;

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
    TickType_t last_sent;
} RFID_CooldownEntry_t;

static RFID_CooldownEntry_t cooldown_entries[RFID_CARD_COOLDOWN_SLOTS];

enum {
    COOLDOWN_SUPPRESSED = -1,
    COOLDOWN_FULL = -2,
    COOLDOWN_INVALID_UID = -3
};

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

static void send_observation(RFID_Status_t status, const CardEvent_t *card, uint8_t suppressed)
{
    ReaderEvent_t event = {0};
    event.status = status;
    event.suppressed = suppressed;
    event.observed_ms = HAL_GetTick();
    if (status == RFID_OK) event.card = *card;

    while (xQueueSend(g_reader_event_queue, &event, pdMS_TO_TICKS(100U)) != pdPASS)
        ++g_rfid_task_stats.queue_waits;
}

static void initialise_reader(void)
{
    do {
        g_rfid_task_stats.init_status = RFID_Init();
        if (g_rfid_task_stats.init_status != RFID_OK) {
            ++g_rfid_task_stats.init_errors;
            send_observation(RFID_ERROR, NULL, 0);
            vTaskDelay(pdMS_TO_TICKS(500U));
        }
    } while (g_rfid_task_stats.init_status != RFID_OK);
}

static void Task_RFID_Read(void *argument)
{
    CardEvent_t card;
    TickType_t last_wake;
    const TickType_t period = pdMS_TO_TICKS(RFID_TASK_PERIOD_MS);
    (void)argument;
    const TickType_t cooldown = pdMS_TO_TICKS(RFID_CARD_COOLDOWN_MS);
    configASSERT(period > 0U);
    configASSERT(sizeof(TickType_t) >= sizeof(uint32_t));
    configASSERT(cooldown > 0U);
    initialise_reader();
    unsigned consecutive_errors = 0;

    last_wake = xTaskGetTickCount();
    for (;;) {
        RFID_Status_t status = RFID_ReadCard(&card);
        g_rfid_task_stats.last_status = status;
        if (status == RFID_OK) {
            ++g_rfid_task_stats.cards_read;
            g_rfid_task_stats.last_card = card;
        } else if (status == RFID_ERROR) {
            ++g_rfid_task_stats.read_errors;
        }
        int slot = COOLDOWN_INVALID_UID;
        uint8_t suppressed = 0;
        if (status == RFID_OK) {
            slot = Cooldown_FindSlot(&card, xTaskGetTickCount(), cooldown);
            if (slot == COOLDOWN_INVALID_UID) {
                status = RFID_ERROR;
                ++g_rfid_task_stats.read_errors;
            } else if (slot < 0) {
                suppressed = 1;
                if (slot == COOLDOWN_FULL) ++g_rfid_task_stats.cooldown_full;
                else ++g_rfid_task_stats.cards_suppressed;
            }
        }
        send_observation(status, status == RFID_OK ? &card : NULL, suppressed);
        if (status == RFID_OK && !suppressed) {
            ++g_rfid_task_stats.cards_queued;
            Cooldown_Record((unsigned)slot, &card, xTaskGetTickCount());
        }
        consecutive_errors = status == RFID_ERROR ? consecutive_errors + 1U : 0U;
        if (consecutive_errors >= 10U) {
            initialise_reader();
            consecutive_errors = 0;
            last_wake = xTaskGetTickCount();
        }
        g_rfid_task_stats.stack_free_words = uxTaskGetStackHighWaterMark(NULL);
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_wake) >= period) last_wake = now;
        vTaskDelayUntil(&last_wake, period);
    }
}

BaseType_t RFID_Task_Start(void)
{
    if (g_reader_event_queue == NULL || rfid_task_handle != NULL) return pdFAIL;
    memset(cooldown_entries, 0, sizeof cooldown_entries);
    return xTaskCreate(Task_RFID_Read, "RFID_Read", RFID_TASK_STACK_WORDS,
                      NULL, RFID_TASK_PRIORITY, &rfid_task_handle);
}
