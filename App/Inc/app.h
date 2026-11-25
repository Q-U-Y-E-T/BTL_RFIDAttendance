#ifndef APP_H
#define APP_H

#include "attendance.h"

void App_Init(void);
void App_Process(void);

AttendanceResult_t App_SubmitCard(const CardEvent_t *card);
AttendanceResult_t App_SubmitCardAt(const CardEvent_t *card, uint32_t observed_ms);
void App_NotifyCardPresent(void);
void App_NotifyNoCard(uint32_t now_ms);
void App_NotifyReaderError(void);

uint8_t App_TakeDisplayEvent(DisplayEvent_t *event);

#endif
