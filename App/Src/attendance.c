#include "attendance.h"
#include "storage_manager.h"
#include "card_cooldown.h"

#include <stddef.h>
#include <string.h>

#define RELEASE_STABLE_MS 600U
#define ABSENCE_MAX_GAP_MS 250U

static uint8_t latched;
static uint32_t last_observed_ms;
static uint8_t last_uid[10];
static uint8_t last_uid_len;
static uint8_t absence_started;
static uint32_t absence_since;
static uint32_t last_absence;

void Attendance_Init(void)
{
    latched = 0;
    absence_started = 0;
    last_uid_len = 0;
}

void Attendance_NotifyNoCard(uint32_t now_ms)
{
    if (!absence_started ||
        (uint32_t)(now_ms - last_absence) > ABSENCE_MAX_GAP_MS) {
        absence_started = 1;
        absence_since = now_ms;
    }
    last_absence = now_ms;
    if ((uint32_t)(now_ms - absence_since) >= RELEASE_STABLE_MS) {
        latched = 0;
    }
}

void Attendance_NotifyCardPresent(void)
{
    absence_started = 0;
}

void Attendance_NotifyReaderError(void)
{
    Attendance_NotifyCardPresent();
}

AttendanceResult_t Attendance_ProcessCard(
    const CardEvent_t *card, DisplayEvent_t *display)
{
    return Attendance_ProcessCardAt(card, display, card ? card->uptime_ms : 0U);
}

AttendanceResult_t Attendance_ProcessCardAt(
    const CardEvent_t *card, DisplayEvent_t *display, uint32_t observed_ms)
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

    absence_started = 0;
    if (latched && last_uid_len == card->uid_len &&
        memcmp(last_uid, card->uid, card->uid_len) == 0 &&
        (uint32_t)(observed_ms - last_observed_ms) < RFID_CARD_COOLDOWN_MS) {
        display->status = APP_READY;
        return ATTENDANCE_DUPLICATE;
    }

    memcpy(last_uid, card->uid, card->uid_len);
    last_uid_len = card->uid_len;
    latched = 1;
    last_observed_ms = observed_ms;

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

    display->status = APP_CARD_ACCEPTED;
    return ATTENDANCE_ACCEPTED;
}
