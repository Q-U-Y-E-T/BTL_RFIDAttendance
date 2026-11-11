#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stdint.h>
#include "storage.h"

void StorageManager_Init(void);
void StorageManager_Process(void);

uint8_t StorageManager_IsReady(void);
Storage_Status_t StorageManager_GetStatus(void);

/*
 * Tra NULL va count = 0 khi danh sach chua san sang.
 * Khong giu con tro qua cac lan goi Process():
 * danh sach co the duoc thay khi cam lai the.
 */
const Student_t *StorageManager_GetStudents(uint16_t *count);

#endif
