#ifndef EMBEDDED_LIST_H
#define EMBEDDED_LIST_H
#include "storage.h"
#include "student_list_update.h"
Storage_Status_t EmbeddedList_Sync(Student_t *scratch, uint16_t capacity);
extern volatile ListUpdate_Result_t g_list_update_result;
extern const char g_embedded_list_version[];
#endif
