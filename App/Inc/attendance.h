#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#include "app_types.h"

typedef enum {
    ATTENDANCE_ACCEPTED = 0,
    ATTENDANCE_UNKNOWN,
    ATTENDANCE_NOT_READY,
    ATTENDANCE_STORAGE_FAILED,
    ATTENDANCE_INVALID_ARGUMENT,
    ATTENDANCE_DUPLICATE,
    ATTENDANCE_BUSY,
    ATTENDANCE_NO_EVENT
} AttendanceResult_t;

AttendanceResult_t Attendance_ProcessCard(
    const CardEvent_t *card,
    DisplayEvent_t *display
);

AttendanceResult_t Attendance_ProcessCardAt(
    const CardEvent_t *card, DisplayEvent_t *display, uint32_t observed_ms);
void Attendance_NotifyCardPresent(void);
void Attendance_Init(void);

void Attendance_NotifyNoCard(uint32_t now_ms);

void Attendance_NotifyReaderError(void);

#endif
