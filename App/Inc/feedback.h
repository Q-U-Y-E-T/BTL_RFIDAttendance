#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stdint.h>
#include "app_types.h"

void Feedback_Init(void);
void Feedback_Process(void);
uint8_t Feedback_IsBusy(void);

/*
 * Tra 1 neu bat dau bao hieu.
 * Tra 0 neu dang ban hoac trang thai khong duoc ho tro.
 * Hien tai chi goi tu cung vong lap main.
 */
uint8_t Feedback_Start(AppStatus_t status);

#endif
