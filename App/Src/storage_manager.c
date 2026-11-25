#include "storage_manager.h"
#include "embedded_list.h"
#include "sd_diskio.h"
#include "diskio.h"
#include "main.h"

#include <stddef.h>

#define SD_CHECK_INTERVAL_MS 1000U

static Student_t students[STORAGE_MAX_STUDENTS];
static uint16_t active_count;

static uint8_t mounted;
static uint8_t list_ready;
static uint8_t list_synced;
static uint32_t last_check;

static Storage_Status_t manager_status = STORAGE_NOT_READY;

void StorageManager_Init(void)
{
    active_count = 0;
    mounted = 0;
    list_ready = 0;
    list_synced = 0;

    manager_status = STORAGE_NOT_READY;

    last_check = HAL_GetTick() - SD_CHECK_INTERVAL_MS;
}

void StorageManager_Process(void)
{
    if ((uint32_t)(HAL_GetTick() - last_check) <
        SD_CHECK_INTERVAL_MS) {
        return;
    }

    if (mounted && !SD_DiskProbe()) {
        mounted = 0;
        list_ready = 0;
        list_synced = 0;
        active_count = 0;

        manager_status = STORAGE_NOT_READY;

        goto finish;
    }

    if (!mounted) {

        if (!SD_DiskTryReset()) {
            manager_status = STORAGE_NOT_READY;
            goto finish;
        }

        if (disk_initialize(0) & STA_NOINIT) {
            manager_status = STORAGE_NOT_READY;
            goto finish;
        }

        manager_status = Storage_Init();

        if (manager_status != STORAGE_OK) {
            goto finish;
        }

        mounted = 1;
        list_synced = 0;
    }

    if (!list_synced) {
        manager_status = EmbeddedList_Sync(students, STORAGE_MAX_STUDENTS);
        if (manager_status != STORAGE_OK) {
            if (manager_status == STORAGE_IO_ERROR || manager_status == STORAGE_NOT_READY)
                mounted = 0;
            goto finish;
        }
        list_synced = 1;
    }

    if (!list_ready) {
        uint16_t pending_count = 0;

        manager_status = Storage_LoadStudents(
            students,
            STORAGE_MAX_STUDENTS,
            &pending_count
        );

        if (manager_status == STORAGE_OK) {
            if (pending_count == 0) {
                manager_status = STORAGE_FORMAT_ERROR;
            } else {
                active_count = pending_count;
                list_ready = 1;
            }
        }

        if (manager_status == STORAGE_IO_ERROR ||
            manager_status == STORAGE_NOT_READY) {
            mounted = 0;
        }
    }

finish:
    last_check = HAL_GetTick();
}

uint8_t StorageManager_IsReady(void)
{
    return list_ready;
}

Storage_Status_t StorageManager_GetStatus(void)
{
    return manager_status;
}

const Student_t *StorageManager_GetStudents(uint16_t *count)
{
    if (count == NULL) {
        return NULL;
    }

    *count = 0;

    if (!list_ready) {
        return NULL;
    }

    *count = active_count;

    return students;
}
void StorageManager_ReportIOError(void)
{
    mounted = 0;
    list_ready = 0;
    list_synced = 0;
    active_count = 0;
    manager_status = STORAGE_IO_ERROR;
    last_check = HAL_GetTick();
}
