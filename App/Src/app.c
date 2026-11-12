#include "app.h"
#include "storage_manager.h"
#include "feedback.h"
#include <stddef.h>

static DisplayEvent_t pending_display;
static uint8_t display_pending;

#ifndef APP_ENABLE_DEMO
#define APP_ENABLE_DEMO 0
#endif

#if APP_ENABLE_DEMO
#include "attendance_demo.h"
#endif

void App_Init(void)
{
    Feedback_Init();
    Attendance_Init();
    display_pending = 0;
    StorageManager_Init();

#if APP_ENABLE_DEMO
    AttendanceDemo_Init();
#endif
}

AttendanceResult_t App_SubmitCard(const CardEvent_t *card)
{
    if (card == NULL ||
        (card->uid_len != 4U && card->uid_len != 7U && card->uid_len != 10U)) {
        Attendance_NotifyReaderError();
        return ATTENDANCE_INVALID_ARGUMENT;
    }

    /* Co the van o do trong khi UI ban: huy chuoi bao vang the. */
    Attendance_NotifyReaderError();
    if (Feedback_IsBusy() || display_pending) {
        return ATTENDANCE_BUSY;
    }

    StorageManager_Process();
    AttendanceResult_t result = Attendance_ProcessCard(card, &pending_display);
    if (result != ATTENDANCE_DUPLICATE &&
        result != ATTENDANCE_INVALID_ARGUMENT) {
        display_pending = 1;
        (void)Feedback_Start(pending_display.status);
    }
    return result;
}

void App_NotifyNoCard(uint32_t now_ms)
{
    Attendance_NotifyNoCard(now_ms);
}

void App_NotifyReaderError(void)
{
    Attendance_NotifyReaderError();
}

uint8_t App_TakeDisplayEvent(DisplayEvent_t *event)
{
    if (event == NULL || !display_pending) {
        return 0;
    }
    *event = pending_display;
    display_pending = 0;
    return 1;
}

void App_Process(void)
{
    Feedback_Process();

    /*
     * Ban vong lap hien tai tam hoan thao tac SD
     * trong luc phat mau bao hieu.
     */
    if (!Feedback_IsBusy()) {
        StorageManager_Process();
    }

#if APP_ENABLE_DEMO
    AttendanceDemo_Process();
#endif
}
