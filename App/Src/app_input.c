#include "app_input.h"
#include "app.h"
#include <stddef.h>

AttendanceResult_t AppInput_Process(const ReaderEvent_t *event)
{
    if (event == NULL) {
        App_NotifyReaderError();
        return ATTENDANCE_INVALID_ARGUMENT;
    }
    switch (event->status) {
    case RFID_OK:
        if (event->suppressed) {
            App_NotifyCardPresent();
            return ATTENDANCE_DUPLICATE;
        }
        return App_SubmitCardAt(&event->card, event->observed_ms);
    case RFID_NO_CARD:
        App_NotifyNoCard(event->observed_ms);
        return ATTENDANCE_NO_EVENT;
    case RFID_ERROR:
        App_NotifyReaderError();
        return ATTENDANCE_NO_EVENT;
    default:
        App_NotifyReaderError();
        return ATTENDANCE_INVALID_ARGUMENT;
    }
}
