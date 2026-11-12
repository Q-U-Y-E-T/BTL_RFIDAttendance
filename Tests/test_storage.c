#include "ff.h"

#include <stdint.h>
#include <string.h>

/* Giu FATFS ton tai sau khi ham test ket thuc. */
static FATFS sd_fs;

/* Co the xem cac bien nay bang debugger.
 * stage: 1 mount, 2 open-write, 3 write, 4 close-write,
 *        5 open-read, 6 read, 7 close-read, 8 compare,
 *        9 success.
 */
volatile uint32_t sd_test_stage;
volatile FRESULT sd_test_fr;
volatile uint8_t sd_test_passed;

uint8_t Test_Storage_Run(void)
{
    static const char message[] = "RFIDAttendance SD OK\r\n";
    char received[sizeof message] = {0};
    FIL file;
    UINT transferred = 0;
    FRESULT operation_result;
    FRESULT close_result;

    sd_test_passed = 0;

    sd_test_stage = 1;
    sd_test_fr = f_mount(&sd_fs, "0:", 1);
    if (sd_test_fr != FR_OK) {
        return 0;
    }

    sd_test_stage = 2;
    sd_test_fr = f_open(
        &file, "0:/SDTEST.TXT", FA_WRITE | FA_CREATE_ALWAYS);
    if (sd_test_fr != FR_OK) {
        return 0;
    }

    sd_test_stage = 3;
    operation_result = f_write(
        &file, message, sizeof message - 1U, &transferred);

    /* Luon dong file, ke ca khi ghi loi. */
    close_result = f_close(&file);

    if (operation_result != FR_OK) {
        sd_test_fr = operation_result;
        return 0;
    }
    if (transferred != sizeof message - 1U) {
        sd_test_fr = FR_DISK_ERR;
        return 0;
    }

    sd_test_stage = 4;
    sd_test_fr = close_result;
    if (sd_test_fr != FR_OK) {
        return 0;
    }

    sd_test_stage = 5;
    sd_test_fr = f_open(&file, "0:/SDTEST.TXT", FA_READ);
    if (sd_test_fr != FR_OK) {
        return 0;
    }

    sd_test_stage = 6;
    transferred = 0;
    operation_result = f_read(
        &file, received, sizeof message - 1U, &transferred);
    close_result = f_close(&file);

    if (operation_result != FR_OK) {
        sd_test_fr = operation_result;
        return 0;
    }
    if (transferred != sizeof message - 1U) {
        sd_test_fr = FR_DISK_ERR;
        return 0;
    }

    sd_test_stage = 7;
    sd_test_fr = close_result;
    if (sd_test_fr != FR_OK) {
        return 0;
    }

    sd_test_stage = 8;
    if (memcmp(message, received, sizeof message - 1U) != 0) {
        sd_test_fr = FR_INT_ERR;
        return 0;
    }

    sd_test_stage = 9;
    sd_test_fr = FR_OK;
    sd_test_passed = 1;
    return 1;
}
