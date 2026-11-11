#include "storage_manager.h"
#include "sd_diskio.h"
#include "diskio.h"
#include "main.h"

#include <stddef.h>

#define SD_CHECK_INTERVAL_MS 1000U

/*
 * Hai mang:
 * - mang dang dung
 * - mang tam de doc danh sach moi
 *
 * Chi doi mang dang dung khi doc va kiem tra thanh cong.
 */
static Student_t lists[2][STORAGE_MAX_STUDENTS];
static uint8_t active_list;
static uint16_t active_count;

static uint8_t mounted;
static uint8_t list_ready;
static uint32_t last_check;

static Storage_Status_t manager_status = STORAGE_NOT_READY;

void StorageManager_Init(void)
{
    active_list = 0;
    active_count = 0;
    mounted = 0;
    list_ready = 0;
    manager_status = STORAGE_NOT_READY;

    /* Cho phep kiem tra ngay lan dau. */
    last_check = HAL_GetTick() - SD_CHECK_INTERVAL_MS;

    StorageManager_Process();
}

void StorageManager_Process(void)
{
    if ((uint32_t)(HAL_GetTick() - last_check) <
        SD_CHECK_INTERVAL_MS) {
        return;
    }

    /*
     * Chi kiem tra khi khong co thao tac SD khac dang chay.
     * Hien tai moi thao tac deu chay tu vong lap main.
     */
    if (mounted && !SD_DiskProbe()) {
        mounted = 0;
        list_ready = 0;
        active_count = 0;
        manager_status = STORAGE_NOT_READY;
    }

    if (!mounted) {
        /*
         * Khoi tao lai the thuc su, khong chi tin trang thai
         * va du lieu cache tu the truoc.
         */
        if (disk_initialize(0) & STA_NOINIT) {
            manager_status = STORAGE_NOT_READY;
            goto finish;
        }

        manager_status = Storage_Init();

        if (manager_status != STORAGE_OK) {
            goto finish;
        }

        mounted = 1;
    }

    if (!list_ready) {
        uint8_t pending_list = active_list ^ 1U;
        uint16_t pending_count = 0;

        manager_status = Storage_LoadStudents(
            lists[pending_list],
            STORAGE_MAX_STUDENTS,
            &pending_count
        );

        if (manager_status == STORAGE_OK) {
            if (pending_count == 0) {
                /* Ung dung diem danh can it nhat mot sinh vien. */
                manager_status = STORAGE_FORMAT_ERROR;
            } else {
                active_list = pending_list;
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
    /* Cho them mot khoang sau khi thao tac SD ket thuc. */
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
    return lists[active_list];
}
