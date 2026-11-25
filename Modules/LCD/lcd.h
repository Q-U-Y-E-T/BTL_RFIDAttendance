#ifndef LCD_H
#define LCD_H

#include "app_types.h"

typedef enum {
    LCD_OK = 0,
    LCD_ERROR
} LCD_Status_t;

LCD_Status_t LCD_Init(void);

LCD_Status_t LCD_ShowEvent(const DisplayEvent_t *event);

#endif
