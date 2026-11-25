#ifndef STUDENT_LIST_UPDATE_H
#define STUDENT_LIST_UPDATE_H
#include <stddef.h>
#include <stdint.h>
#include "storage.h"
typedef enum {
    LIST_UPDATE_CREATED = 0,
    LIST_UPDATE_UPDATED,
    LIST_UPDATE_UNCHANGED,
    LIST_UPDATE_EXISTS,
    LIST_UPDATE_INVALID,
    LIST_UPDATE_IO_ERROR,
    LIST_UPDATE_NO_BACKUP_SLOT
} ListUpdate_Result_t;
ListUpdate_Result_t ListUpdate_Apply(const uint8_t *data, size_t size,
    uint16_t expected_count, uint8_t replace, Student_t *scratch, uint16_t capacity);
#endif
