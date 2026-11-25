#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include "app_types.h"

#define STORAGE_MAX_STUDENTS 50U

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

Storage_Status_t Storage_Init(void);

Storage_Status_t Storage_LoadStudents(
    Student_t *students,
    uint16_t capacity,
    uint16_t *count
);

Storage_Status_t Storage_AppendAttendance(
    const CardEvent_t *card,
    const Student_t *student
);

Storage_Status_t Storage_LoadStudentsFile(
    const char *path, Student_t *students, uint16_t capacity, uint16_t *count
);

#endif
