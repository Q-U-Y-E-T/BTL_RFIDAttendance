#include "app.h"
#include "storage_manager.h"
#include "feedback.h"
#include <stddef.h>

static DisplayEvent_t pending_display;
static uint8_t display_pending;

void App_Init(void)
{
    Feedback_Init();
    Attendance_Init();
    display_pending = 0;
    StorageManager_Init();
}

AttendanceResult_t App_SubmitCard(const CardEvent_t *card)
{
    return App_SubmitCardAt(card, card ? card->uptime_ms : 0U);
}

AttendanceResult_t App_SubmitCardAt(const CardEvent_t *card, uint32_t observed_ms)
{
    if (card == NULL ||
        (card->uid_len != 4U && card->uid_len != 7U && card->uid_len != 10U)) {
        Attendance_NotifyReaderError();
        return ATTENDANCE_INVALID_ARGUMENT;
    }

    Attendance_NotifyCardPresent();
    if (display_pending) {
        return ATTENDANCE_BUSY;
    }

    StorageManager_Process();
    AttendanceResult_t result = Attendance_ProcessCardAt(card, &pending_display, observed_ms);
    if (result != ATTENDANCE_DUPLICATE &&
        result != ATTENDANCE_INVALID_ARGUMENT) {
        display_pending = 1;
    }
    return result;
}

void App_NotifyCardPresent(void)
{
    Attendance_NotifyCardPresent();
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
    StorageManager_Process();
}
