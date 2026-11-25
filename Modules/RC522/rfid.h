#ifndef RFID_H
#define RFID_H

#include "app_types.h"

typedef enum {
    RFID_OK = 0,
    RFID_NO_CARD,
    RFID_ERROR
} RFID_Status_t;

RFID_Status_t RFID_Init(void);

RFID_Status_t RFID_ReadCard(CardEvent_t *card);

#endif
