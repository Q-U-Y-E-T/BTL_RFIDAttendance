#include "ff.h"
#include <stdint.h>

/* Du lieu gia de test, khong phai UID the that. */
static const char sample_csv[] =
    "uid,student_id,student_name\r\n"
    "A1B2C3D4,SV001,NGUYEN VAN A\r\n"
    "11223344,SV002,TRAN THI B\r\n"
    "00112233,SV003,LE VAN C\r\n";

/* Xem bang debugger neu can chan doan. */
volatile FRESULT seed_students_fr;

/*
 * Goi sau khi Test_Storage_Run() thanh cong:
 * he thong file luc do da duoc mount.
 *
 * Chi tao khi chua co DanhSach.csv.
 * Neu da co file, giu nguyen noi dung.
 */
uint8_t Test_Storage_CreateSampleList(void)
{
    FIL file;
    UINT written = 0;
    FRESULT write_result;
    FRESULT close_result;

    seed_students_fr = f_open(
        &file,
        "0:/DanhSach.csv",
        FA_WRITE | FA_CREATE_NEW
    );

    if (seed_students_fr == FR_EXIST) {
        seed_students_fr = FR_OK;
        return 1;
    }

    if (seed_students_fr != FR_OK) {
        return 0;
    }

    write_result = f_write(
        &file,
        sample_csv,
        (UINT)(sizeof sample_csv - 1U),
        &written
    );

    /* Dong file de day du lieu con trong bo dem xuong the. */
    close_result = f_close(&file);

    if (write_result != FR_OK) {
        seed_students_fr = write_result;
        return 0;
    }

    if (written != (UINT)(sizeof sample_csv - 1U)) {
        seed_students_fr = FR_DISK_ERR;
        return 0;
    }

    seed_students_fr = close_result;
    return close_result == FR_OK;
}

