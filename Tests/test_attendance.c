#include "storage.h"
#include "ff.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

volatile uint32_t attendance_test_stage;
volatile Storage_Status_t attendance_test_result;
volatile FRESULT attendance_test_fr;

uint8_t Test_Attendance_Run(
    const Student_t *students,
    uint16_t count)
{
    static const char header[] = "uptime_ms,uid,student_id\r\n";
    static const char hex[] = "0123456789ABCDEF";

    FIL file;
    DWORD previous_size = 0;
    CardEvent_t card = {0};
    char uid_text[21];
    char expected[64];
    char actual[64] = {0};
    UINT received = 0;
    FRESULT result;
    FRESULT close_result;

    attendance_test_stage = 1;
    attendance_test_result = STORAGE_INVALID_ARGUMENT;

    if (students == NULL || count == 0 ||
        (students[0].uid_len != 4U &&
         students[0].uid_len != 7U &&
         students[0].uid_len != 10U) ||
        memchr(students[0].student_id, '\0',
               sizeof students[0].student_id) == NULL) {
        return 0;
    }

    /* Ghi nho kich thuoc lich su cu. */
    attendance_test_fr = f_open(&file, "0:/history.csv", FA_READ);

    if (attendance_test_fr == FR_OK) {
        previous_size = f_size(&file);
        attendance_test_fr = f_close(&file);

        if (attendance_test_fr != FR_OK) {
            return 0;
        }
    } else if (attendance_test_fr != FR_NO_FILE) {
        return 0;
    }

    /*
     * Gia lap the cua sinh vien dau tien.
     * 12345 la moc ms gia de test, khong phai gio thuc.
     */
    card.uid_len = students[0].uid_len;
    memcpy(card.uid, students[0].uid, card.uid_len);
    card.uptime_ms = 12345U;

    for (uint8_t i = 0; i < card.uid_len; ++i) {
        uid_text[2U * i] = hex[card.uid[i] >> 4];
        uid_text[2U * i + 1U] = hex[card.uid[i] & 0x0FU];
    }
    uid_text[2U * card.uid_len] = '\0';

    int length = snprintf(
        expected, sizeof expected, "%lu,%s,%s\r\n",
        (unsigned long)card.uptime_ms,
        uid_text,
        students[0].student_id
    );

    if (length <= 0 || (size_t)length >= sizeof expected) {
        return 0;
    }

    attendance_test_stage = 2;
    attendance_test_result =
        Storage_AppendAttendance(&card, &students[0]);

    if (attendance_test_result != STORAGE_OK) {
        return 0;
    }

    attendance_test_stage = 3;
    attendance_test_fr = f_open(&file, "0:/history.csv", FA_READ);

    if (attendance_test_fr != FR_OK) {
        return 0;
    }

    DWORD expected_size = previous_size + (DWORD)length;

    if (previous_size == 0) {
        expected_size += (DWORD)(sizeof header - 1U);
    }

    uint8_t passed = 0;

    /* Kiem tra kich thuoc tang dung, khong bi ghi de file. */
    if (f_size(&file) == expected_size) {
        attendance_test_stage = 4;

        result = f_lseek(
            &file, f_size(&file) - (DWORD)length);

        if (result == FR_OK) {
            result = f_read(
                &file, actual, (UINT)length, &received);
        }

        attendance_test_fr = result;

        if (result == FR_OK &&
            received == (UINT)length &&
            memcmp(actual, expected, (size_t)length) == 0) {
            passed = 1;
        }
    }

    close_result = f_close(&file);

    if (close_result != FR_OK) {
        attendance_test_fr = close_result;
        passed = 0;
    }

    if (passed) {
        attendance_test_stage = 5;
    }

    return passed;
}

