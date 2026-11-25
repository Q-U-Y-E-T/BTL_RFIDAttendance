#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
    uint32_t uptime_ms;
} CardEvent_t;

typedef enum {
    APP_READY,
    APP_CARD_ACCEPTED,
    APP_CARD_UNKNOWN,
    APP_STORAGE_ERROR,
    APP_LIST_ERROR,
    APP_READER_ERROR
} AppStatus_t;

typedef struct {
    AppStatus_t status;
    char student_id[16];
    char student_name[49];
} DisplayEvent_t;

#endif
