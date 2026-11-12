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
    ATTENDANCE_BUSY
} AttendanceResult_t;

/*
 * Xu ly mot luot quet.
 * Dien ket qua cho LCD vao display.
 * Khong giu lai con tro sau khi tra ve.
 *
 * Chi goi tu cung luong xu ly voi StorageManager hien tai.
 * Cung UID bi chan cho den khi bao vang the lien tuc du thoi gian.
 * Ca luot ghi loi cung duoc chan lap: khong tu dong ghi lai.
 */
AttendanceResult_t Attendance_ProcessCard(
    const CardEvent_t *card,
    DisplayEvent_t *display
);

void Attendance_Init(void);
/* Bao RFID_NO_CARD moi lan poll (khoang 100 ms hoac nhanh hon). */
void Attendance_NotifyNoCard(uint32_t now_ms);
/* Loi dau doc khong duoc coi la da rut the. */
void Attendance_NotifyReaderError(void);

#endif
