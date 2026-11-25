#ifndef READER_EVENT_H
#define READER_EVENT_H
#include "rfid.h"

typedef struct {
    RFID_Status_t status;
    CardEvent_t card;
    uint32_t observed_ms;
    uint8_t suppressed;
} ReaderEvent_t;
#endif
