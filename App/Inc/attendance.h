#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#include "app_types.h"

typedef enum {
    ATTENDANCE_ACCEPTED = 0,
    ATTENDANCE_UNKNOWN,
    ATTENDANCE_NOT_READY,
    ATTENDANCE_STORAGE_FAILED,
    ATTENDANCE_INVALID_ARGUMENT
} AttendanceResult_t;

/*
 * Xu ly mot luot quet.
 * Dien ket qua cho LCD vao display.
 * Khong giu lai con tro sau khi tra ve.
 *
 * Chi goi tu cung luong xu ly voi StorageManager hien tai.
 * Chua xu ly chong quet lap trong ham nay.
 */
AttendanceResult_t Attendance_ProcessCard(
    const CardEvent_t *card,
    DisplayEvent_t *display
);

#endif
