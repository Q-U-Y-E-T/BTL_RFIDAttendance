#include "embedded_list.h"
#include "embedded_list_data.h"

volatile ListUpdate_Result_t g_list_update_result = LIST_UPDATE_IO_ERROR;
const char g_embedded_list_version[] = EMBEDDED_LIST_VERSION;

Storage_Status_t EmbeddedList_Sync(Student_t *scratch, uint16_t capacity)
{
    g_list_update_result = ListUpdate_Apply(embedded_csv, sizeof embedded_csv,
        EMBEDDED_STUDENT_COUNT, 1U, scratch, capacity);
    switch (g_list_update_result) {
    case LIST_UPDATE_CREATED:
    case LIST_UPDATE_UPDATED:
    case LIST_UPDATE_UNCHANGED:
        return STORAGE_OK;
    case LIST_UPDATE_NO_BACKUP_SLOT:
        return STORAGE_CAPACITY_ERROR;
    case LIST_UPDATE_INVALID:
    case LIST_UPDATE_EXISTS:
        return STORAGE_FORMAT_ERROR;
    default:
        return STORAGE_IO_ERROR;
    }
}
