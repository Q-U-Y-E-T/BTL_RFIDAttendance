#include "attendance.h"
#include "storage_manager.h"

#include <stddef.h>
#include <string.h>

AttendanceResult_t Attendance_ProcessCard(
    const CardEvent_t *card,
    DisplayEvent_t *display)
{
    if (display == NULL) {
        return ATTENDANCE_INVALID_ARGUMENT;
    }

    memset(display, 0, sizeof *display);
    display->status = APP_CARD_UNKNOWN;

    if (card == NULL ||
        (card->uid_len != 4U &&
         card->uid_len != 7U &&
         card->uid_len != 10U)) {
        return ATTENDANCE_INVALID_ARGUMENT;
    }

    uint16_t count = 0;
    const Student_t *students =
        StorageManager_GetStudents(&count);

    if (students == NULL || count == 0) {
        Storage_Status_t status = StorageManager_GetStatus();

        if (status == STORAGE_FILE_NOT_FOUND ||
            status == STORAGE_FORMAT_ERROR ||
            status == STORAGE_CAPACITY_ERROR) {
            display->status = APP_LIST_ERROR;
        } else {
            display->status = APP_STORAGE_ERROR;
        }

        return ATTENDANCE_NOT_READY;
    }

    const Student_t *matched = NULL;

    for (uint16_t i = 0; i < count; ++i) {
        if (students[i].uid_len == card->uid_len &&
            memcmp(students[i].uid, card->uid,
                   card->uid_len) == 0) {
            matched = &students[i];
            break;
        }
    }

    if (matched == NULL) {
        display->status = APP_CARD_UNKNOWN;
        return ATTENDANCE_UNKNOWN;
    }

    memcpy(
        display->student_id,
        matched->student_id,
        sizeof display->student_id
    );

    memcpy(
        display->student_name,
        matched->student_name,
        sizeof display->student_name
    );

    display->student_id[sizeof display->student_id - 1U] = '\0';
    display->student_name[sizeof display->student_name - 1U] = '\0';

    Storage_Status_t status =
        Storage_AppendAttendance(card, matched);

    if (status != STORAGE_OK) {
        display->status = APP_STORAGE_ERROR;
        StorageManager_ReportIOError();

        return ATTENDANCE_STORAGE_FAILED;
    }

    /* Chi bao thanh cong sau khi ghi log thanh cong. */
    display->status = APP_CARD_ACCEPTED;
    return ATTENDANCE_ACCEPTED;
}
