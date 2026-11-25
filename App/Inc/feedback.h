#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stdint.h>
#include "app_types.h"

void Feedback_Init(void);
void Feedback_Process(void);
uint8_t Feedback_IsBusy(void);

uint8_t Feedback_Start(AppStatus_t status);

#endif
