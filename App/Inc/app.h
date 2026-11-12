#ifndef APP_H
#define APP_H

#include "attendance.h"

void App_Init(void);
void App_Process(void);

/* Cac ham sau chi goi tu cung vong lap/task so huu App. */
AttendanceResult_t App_SubmitCard(const CardEvent_t *card);
void App_NotifyNoCard(uint32_t now_ms);
void App_NotifyReaderError(void);
/* Lay mot ket qua cho LCD. Tra 0 neu chua co ket qua moi. */
uint8_t App_TakeDisplayEvent(DisplayEvent_t *event);

#endif
