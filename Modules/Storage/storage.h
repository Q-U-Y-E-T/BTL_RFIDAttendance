#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include "app_types.h"

#define STORAGE_MAX_STUDENTS 50U

/* Mot sinh vien trong DanhSach.csv */
typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
    char student_id[16];
    char student_name[49];
} Student_t;

typedef enum {
    STORAGE_OK = 0,
    STORAGE_INVALID_ARGUMENT,
    STORAGE_IO_ERROR,
    STORAGE_FILE_NOT_FOUND,
    STORAGE_FORMAT_ERROR,
    STORAGE_CAPACITY_ERROR,
    STORAGE_NOT_READY
} Storage_Status_t;

/*
 * Goi sau MX_GPIO_Init() va MX_SPI2_Init().
 * Khoi tao the SD va mount he thong file.
 * Chi tra STORAGE_OK khi he thong file san sang.
 * Ham phai co thoi han cho, khong cho vo han.
 */
Storage_Status_t Storage_Init(void);

/*
 * Doc DanhSach.csv vao mang students do ben goi cap phat.
 *
 * students va count phai khac NULL.
 * capacity la so phan tu cua mang, tu 1 den
 * STORAGE_MAX_STUDENTS.
 *
 * Thanh cong: *count la so sinh vien da doc.
 * That bai: *count = 0 neu count khac NULL.
 * Khong su dung noi dung mang khi ham that bai.
 *
 * UID trung, UID sai, chuoi qua dai hoac CSV sai:
 * tra STORAGE_FORMAT_ERROR.
 *
 * So sinh vien vuot capacity:
 * tra STORAGE_CAPACITY_ERROR, khong cat bot danh sach.
 *
 * Chi goi sau khi Storage_Init() thanh cong.
 */
Storage_Status_t Storage_LoadStudents(
    Student_t *students,
    uint16_t capacity,
    uint16_t *count
);

/*
 * Them mot luot diem danh vao history.csv.
 * Tao file va dong tieu de neu file chua ton tai/rong.
 *
 * card va student phai khac NULL, UID phai khop nhau.
 * Ghi uptime_ms, UID va ma sinh vien.
 * uptime_ms khong phai ngay gio thuc.
 *
 * Khong giu lai cac con tro sau khi ham ket thuc.
 * Chi tra STORAGE_OK khi ghi va dong bo/dong file
 * thanh cong.
 *
 * Neu loi ghi, khong tu dong thu lai vo han.
 * Chi goi sau khi Storage_Init() thanh cong.
 */
Storage_Status_t Storage_AppendAttendance(
    const CardEvent_t *card,
    const Student_t *student
);

#endif /* STORAGE_H */
