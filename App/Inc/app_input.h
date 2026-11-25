#ifndef APP_INPUT_H
#define APP_INPUT_H
#include "reader_event.h"
#include "attendance.h"

AttendanceResult_t AppInput_Process(const ReaderEvent_t *event);
#endif
