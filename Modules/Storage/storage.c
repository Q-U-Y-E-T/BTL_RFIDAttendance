#include "storage.h"
#include "ff.h"

#include <stddef.h>
#include <string.h>

static FATFS storage_fs;
static uint8_t storage_ready;

/* Chuyen loi FatFs sang loi giao dien Storage. */
static Storage_Status_t map_error(FRESULT result)
{
    switch (result) {
    case FR_OK:
        return STORAGE_OK;

    case FR_NO_FILE:
    case FR_NO_PATH:
        return STORAGE_FILE_NOT_FOUND;

    case FR_NOT_READY:
    case FR_NOT_ENABLED:
        return STORAGE_NOT_READY;

    default:
        return STORAGE_IO_ERROR;
    }
}

Storage_Status_t Storage_Init(void)
{
    storage_ready = 0;

    FRESULT result = f_mount(&storage_fs, "0:", 1);

    if (result != FR_OK) {
        return map_error(result);
    }

    storage_ready = 1;
    return STORAGE_OK;
}

/*
 * Doc mot dong, chap nhan LF hoac CRLF.
 * Ket qua:
 *   1: doc duoc mot dong
 *   0: het file
 *  -1: dong qua dai hoac co byte NUL
 *  -2: loi doc SD
 */
static int read_line(FIL *file, char *line, size_t capacity)
{
    size_t used = 0;

    for (;;) {
        char ch;
        UINT received = 0;

        FRESULT result = f_read(file, &ch, 1, &received);

        if (result != FR_OK) {
            return -2;
        }

        if (received == 0) {
            if (used == 0) {
                line[0] = '\0';
                return 0;
            }
            break;
        }

        if (ch == '\n') {
            break;
        }

        if (ch == '\0' || used >= capacity - 1U) {
            return -1;
        }

        line[used++] = ch;
    }

    if (used > 0 && line[used - 1U] == '\r') {
        --used;
    }

    line[used] = '\0';
    return 1;
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    return -1;
}

/* Chi nhan ASCII in duoc; ten cho phep dau cach. */
static int valid_text(const char *text, int allow_space)
{
    if (*text == '\0') {
        return 0;
    }

    for (; *text != '\0'; ++text) {
        unsigned char ch = (unsigned char)*text;

        if (ch < (allow_space ? 32U : 33U) ||
            ch > 126U || ch == ',') {
            return 0;
        }
    }

    return 1;
}

static int parse_student(char *line, Student_t *student)
{
    char *first_comma = strchr(line, ',');
    if (first_comma == NULL) {
        return 0;
    }

    char *second_comma = strchr(first_comma + 1, ',');
    if (second_comma == NULL ||
        strchr(second_comma + 1, ',') != NULL) {
        return 0;
    }

    *first_comma = '\0';
    *second_comma = '\0';

    const char *uid_text = line;
    const char *id_text = first_comma + 1;
    const char *name_text = second_comma + 1;

    size_t uid_chars = strlen(uid_text);
    size_t id_chars = strlen(id_text);
    size_t name_chars = strlen(name_text);

    /* UID RFID: 4, 7 hoac 10 byte. */
    if (uid_chars != 8U &&
        uid_chars != 14U &&
        uid_chars != 20U) {
        return 0;
    }

    if (id_chars >= sizeof student->student_id ||
        name_chars >= sizeof student->student_name ||
        !valid_text(id_text, 0) ||
        !valid_text(name_text, 1)) {
        return 0;
    }

    memset(student, 0, sizeof *student);
    student->uid_len = (uint8_t)(uid_chars / 2U);

    for (size_t i = 0; i < student->uid_len; ++i) {
        int high = hex_value(uid_text[2U * i]);
        int low = hex_value(uid_text[2U * i + 1U]);

        if (high < 0 || low < 0) {
            return 0;
        }

        student->uid[i] = (uint8_t)((high << 4) | low);
    }

    memcpy(student->student_id, id_text, id_chars + 1U);
    memcpy(student->student_name, name_text, name_chars + 1U);

    return 1;
}

Storage_Status_t Storage_LoadStudents(
    Student_t *students,
    uint16_t capacity,
    uint16_t *count)
{
    FIL file;
    char line[96];
    uint16_t loaded = 0;
    Storage_Status_t status = STORAGE_OK;

    if (count != NULL) {
        *count = 0;
    }

    if (students == NULL || count == NULL ||
        capacity == 0 || capacity > STORAGE_MAX_STUDENTS) {
        return STORAGE_INVALID_ARGUMENT;
    }

    if (!storage_ready) {
        return STORAGE_NOT_READY;
    }

    FRESULT result = f_open(&file, "0:/DanhSach.csv", FA_READ);
    if (result != FR_OK) {
        return map_error(result);
    }

    int line_result = read_line(&file, line, sizeof line);

    if (line_result == -2) {
        status = STORAGE_IO_ERROR;
        goto finish;
    }

    if (line_result != 1 ||
        strcmp(line, "uid,student_id,student_name") != 0) {
        status = STORAGE_FORMAT_ERROR;
        goto finish;
    }

    for (;;) {
        Student_t candidate;

        line_result = read_line(&file, line, sizeof line);

        if (line_result == 0) {
            break;
        }

        if (line_result == -2) {
            status = STORAGE_IO_ERROR;
            break;
        }

        if (line_result != 1 || !parse_student(line, &candidate)) {
            status = STORAGE_FORMAT_ERROR;
            break;
        }

        /* UID phai duy nhat trong danh sach. */
        for (uint16_t i = 0; i < loaded; ++i) {
            if (students[i].uid_len == candidate.uid_len &&
                memcmp(students[i].uid, candidate.uid,
                       candidate.uid_len) == 0) {
                status = STORAGE_FORMAT_ERROR;
                goto finish;
            }
        }

        if (loaded >= capacity) {
            status = STORAGE_CAPACITY_ERROR;
            break;
        }

        students[loaded++] = candidate;
    }

finish:
    result = f_close(&file);

    if (status == STORAGE_OK && result != FR_OK) {
        status = map_error(result);
    }

    if (status == STORAGE_OK) {
        *count = loaded;
    }

    return status;
}

