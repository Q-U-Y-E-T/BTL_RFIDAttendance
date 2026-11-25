#include "student_list_update.h"
#include "storage.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define TARGET "0:/DanhSach.csv"
#define STAGED "0:/DSNEW.CSV"

static FRESULT matches_file(const char *path, const uint8_t *data, size_t size,
                            uint8_t *matches)
{
    FIL file;
    uint8_t block[64];
    *matches = 0;
    FRESULT result = f_open(&file, path, FA_READ);
    if (result != FR_OK) return result;
    uint8_t equal = f_size(&file) == size;
    size_t offset = 0;
    while (equal && offset < size) {
        UINT wanted = size-offset > sizeof block ? sizeof block : (UINT)(size-offset);
        UINT received = 0;
        result = f_read(&file, block, wanted, &received);
        if (result != FR_OK) break;
        if (received != wanted || memcmp(block, data+offset, wanted) != 0) equal = 0;
        offset += wanted;
    }
    FRESULT closed = f_close(&file);
    if (result == FR_OK) result = closed;
    if (result == FR_OK) *matches = equal;
    return result;
}

static ListUpdate_Result_t validate_file(const char *path, uint16_t expected_count,
                                         Student_t *scratch, uint16_t capacity)
{
    uint16_t count = 0;
    Storage_Status_t status = Storage_LoadStudentsFile(
        path, scratch, capacity, &count);
    if (status == STORAGE_OK && count == expected_count) return LIST_UPDATE_UNCHANGED;
    if (status == STORAGE_FORMAT_ERROR || status == STORAGE_CAPACITY_ERROR || status == STORAGE_OK)
        return LIST_UPDATE_INVALID;
    return LIST_UPDATE_IO_ERROR;
}

static ListUpdate_Result_t reserve_name(const char *prefix, char out[32])
{
    FILINFO info;
    for (unsigned i = 0; i < 100U; ++i) {
        (void)snprintf(out, 32, "0:/%s%02u.CSV", prefix, i);
        FRESULT result = f_stat(out, &info);
        if (result == FR_NO_FILE) return LIST_UPDATE_UNCHANGED;
        if (result != FR_OK) return LIST_UPDATE_IO_ERROR;
    }
    return LIST_UPDATE_NO_BACKUP_SLOT;
}

ListUpdate_Result_t ListUpdate_Apply(const uint8_t *data, size_t size,
                                  uint16_t expected_count, uint8_t replace,
                                 Student_t *scratch, uint16_t capacity)
{
    FILINFO info;
    FIL file;
    char backup[32] = {0};
    char previous_stage[32];
    uint8_t same = 0;
    if (data == NULL || size == 0 || size > 8192U ||
        expected_count == 0 || expected_count > STORAGE_MAX_STUDENTS || replace > 1U ||
        scratch == NULL || capacity < expected_count || capacity > STORAGE_MAX_STUDENTS)
        return LIST_UPDATE_INVALID;

    FRESULT result = f_stat(TARGET, &info);
    uint8_t exists = result == FR_OK;
    if (exists) {
        if (info.fattrib & AM_DIR) return LIST_UPDATE_IO_ERROR;
        if (matches_file(TARGET, data, size, &same) != FR_OK) return LIST_UPDATE_IO_ERROR;
        if (same) return validate_file(TARGET, expected_count, scratch, capacity);
        if (!replace) return LIST_UPDATE_EXISTS;
    } else if (result != FR_NO_FILE) return LIST_UPDATE_IO_ERROR;

    result = f_stat(STAGED, &info);
    if (result == FR_OK) {
        if (info.fattrib & AM_DIR) return LIST_UPDATE_IO_ERROR;
        if (matches_file(STAGED, data, size, &same) != FR_OK) return LIST_UPDATE_IO_ERROR;
        if (!same) {
            ListUpdate_Result_t reserved = reserve_name("TMPBAK", previous_stage);
            if (reserved != LIST_UPDATE_UNCHANGED) return reserved;
            if (f_rename(STAGED, previous_stage) != FR_OK) return LIST_UPDATE_IO_ERROR;
        }
    } else if (result != FR_NO_FILE) return LIST_UPDATE_IO_ERROR;

    if (!same) {
        result = f_open(&file, STAGED, FA_WRITE | FA_CREATE_NEW);
        if (result != FR_OK) return LIST_UPDATE_IO_ERROR;
        UINT written = 0;
        result = f_write(&file, data, (UINT)size, &written);
        if (result == FR_OK && written == size) result = f_sync(&file);
        else if (result == FR_OK) result = FR_DISK_ERR;
        FRESULT closed = f_close(&file);
        if (result == FR_OK) result = closed;
        if (result != FR_OK) return LIST_UPDATE_IO_ERROR;
    }
    if (matches_file(STAGED, data, size, &same) != FR_OK || !same)
        return LIST_UPDATE_IO_ERROR;
    ListUpdate_Result_t validated = validate_file(STAGED, expected_count, scratch, capacity);
    if (validated != LIST_UPDATE_UNCHANGED) return validated;

    if (exists) {
        ListUpdate_Result_t reserved = reserve_name("DSBAK", backup);
        if (reserved != LIST_UPDATE_UNCHANGED) return reserved;
        if (f_rename(TARGET, backup) != FR_OK) return LIST_UPDATE_IO_ERROR;
    }
    if (f_rename(STAGED, TARGET) != FR_OK) {
        if (exists) (void)f_rename(backup, TARGET);
        return LIST_UPDATE_IO_ERROR;
    }
    if (matches_file(TARGET, data, size, &same) != FR_OK || !same)
        return LIST_UPDATE_IO_ERROR;
    validated = validate_file(TARGET, expected_count, scratch, capacity);
    if (validated != LIST_UPDATE_UNCHANGED) return validated;
    return exists ? LIST_UPDATE_UPDATED : LIST_UPDATE_CREATED;
}
