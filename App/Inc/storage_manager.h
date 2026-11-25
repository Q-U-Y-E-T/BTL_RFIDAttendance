#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stdint.h>
#include "storage.h"

void StorageManager_Init(void);
void StorageManager_Process(void);

uint8_t StorageManager_IsReady(void);
Storage_Status_t StorageManager_GetStatus(void);

const Student_t *StorageManager_GetStudents(uint16_t *count);
void StorageManager_ReportIOError(void);
#endif
