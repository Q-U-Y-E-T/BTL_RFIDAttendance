#include "attendance_demo.h"
#include "app.h"
#include "attendance.h"
#include "storage_manager.h"
#include "feedback.h"
#include "main.h"

#include <stddef.h>
#include <string.h>

static uint8_t demo_stage;
static uint32_t demo_tick;

static CardEvent_t demo_card;

/* Xem bang debugger */
volatile AttendanceResult_t demo_valid_result;
volatile AttendanceResult_t demo_unknown_result;
volatile AttendanceResult_t demo_duplicate_result;

static uint8_t make_unknown_card(CardEvent_t *card)
{
    uint16_t count = 0;

    const Student_t *students =
        StorageManager_GetStudents(&count);

    if (students == NULL) {
        return 0;
    }

    memset(card, 0, sizeof *card);
    card->uid_len = 4;

    for (unsigned int value = 0; value < 256U; ++value) {
        uint8_t found = 0;

        card->uid[0] = (uint8_t)value;

        for (uint16_t i = 0; i < count; ++i) {
            if (students[i].uid_len == card->uid_len &&
                memcmp(students[i].uid,
                       card->uid,
                       card->uid_len) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            return 1;
        }
    }

    return 0;
}

void AttendanceDemo_Init(void)
{
    demo_stage = 0;
    demo_tick = HAL_GetTick();

    demo_valid_result = ATTENDANCE_NOT_READY;
    demo_unknown_result = ATTENDANCE_NOT_READY;
    demo_duplicate_result = ATTENDANCE_NOT_READY;
}

void AttendanceDemo_Process(void)
{
    if (Feedback_IsBusy() || !StorageManager_IsReady()) {
        return;
    }

    if (demo_stage == 0 &&
        (uint32_t)(HAL_GetTick() - demo_tick) >= 2000U) {
        uint16_t count = 0;

        const Student_t *students =
            StorageManager_GetStudents(&count);

        if (students == NULL || count == 0) {
            return;
        }

        memset(&demo_card, 0, sizeof demo_card);

        demo_card.uid_len = students[0].uid_len;
        memcpy(demo_card.uid, students[0].uid, demo_card.uid_len);
        demo_card.uptime_ms = HAL_GetTick();

        demo_valid_result = App_SubmitCard(&demo_card);
        if (demo_valid_result == ATTENDANCE_BUSY) {
            return;
        }
        /* Ket qua de vong lap chinh lay va dua len LCD (lcd_display). */

        demo_stage = 1;
        demo_tick = HAL_GetTick();

        return;
    }

    /* Giu nguyen the: khong ghi them log va khong phat mau bao moi. */
    if (demo_stage == 1) {
        demo_card.uptime_ms = HAL_GetTick();
        demo_duplicate_result = App_SubmitCard(&demo_card);
        if (demo_duplicate_result == ATTENDANCE_BUSY) {
            return;
        }
        /* Ket qua de vong lap chinh lay va dua len LCD (lcd_display). */
        demo_stage = 2;
        return;
    }

    if (demo_stage == 2 &&
        (uint32_t)(HAL_GetTick() - demo_tick) >= 3000U) {
        if (!make_unknown_card(&demo_card)) {
            return;
        }

        demo_card.uptime_ms = HAL_GetTick();

        demo_unknown_result = App_SubmitCard(&demo_card);
        if (demo_unknown_result == ATTENDANCE_BUSY) {
            return;
        }
        /* Ket qua de vong lap chinh lay va dua len LCD (lcd_display). */

        demo_stage = 3;
    }
}
