#include "app_rtos.h"
#include "app.h"
#include "app_input.h"
#include "app_queues.h"
#include "task_rfid_read.h"
#include "storage_manager.h"
#include "feedback.h"
#include "lcd.h"
#include "main.h"
#include <string.h>

#define APP_STACK_WORDS 768U
#define UI_STACK_WORDS 384U
#define RESULT_HOLD_MS 2000U

volatile AppRTOS_Stats_t g_app_rtos_stats;
const char * volatile g_rtos_fault_file;
volatile int g_rtos_fault_line;

void app_assert_failed(const char *file, int line)
{
    g_rtos_fault_file = file;
    g_rtos_fault_line = line;
    __disable_irq();

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    for (;;) { __NOP(); }
}
void vApplicationMallocFailedHook(void)
{ app_assert_failed("FreeRTOS heap exhausted", 0); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{ (void)task; app_assert_failed(name, 0); }

static AppStatus_t system_status(uint8_t reader_error)
{
    if (reader_error) return APP_READER_ERROR;
    if (StorageManager_IsReady()) return APP_READY;
    Storage_Status_t status = StorageManager_GetStatus();
    if (status == STORAGE_FILE_NOT_FOUND || status == STORAGE_FORMAT_ERROR ||
        status == STORAGE_CAPACITY_ERROR) return APP_LIST_ERROR;
    return APP_STORAGE_ERROR;
}

static void Task_Storage_App(void *argument)
{
    ReaderEvent_t input;
    DisplayEvent_t display;
    uint8_t reader_error = 0;
    uint8_t have_input = 0;
    (void)argument;
    for (;;) {
        App_Process();
        if (!have_input)
            have_input = xQueueReceive(g_reader_event_queue, &input, 0) == pdPASS;
        if (have_input) {
            AttendanceResult_t result = AppInput_Process(&input);
            if (result != ATTENDANCE_BUSY) {
                have_input = 0;
                ++g_app_rtos_stats.input_events;
                reader_error = input.status == RFID_ERROR;
                if (result == ATTENDANCE_ACCEPTED) ++g_app_rtos_stats.accepted;
                if (result == ATTENDANCE_DUPLICATE) ++g_app_rtos_stats.duplicates;
                if (result == ATTENDANCE_UNKNOWN) ++g_app_rtos_stats.unknown;
                if (result == ATTENDANCE_STORAGE_FAILED) ++g_app_rtos_stats.storage_failures;
            }
        }
        if (App_TakeDisplayEvent(&display)) {

            while (xQueueSend(g_display_event_queue, &display,
                             pdMS_TO_TICKS(100U)) != pdPASS)
                ++g_app_rtos_stats.display_waits;
        }
        memset(&display, 0, sizeof display);
        display.status = system_status(reader_error);
        (void)xQueueOverwrite(g_system_display_queue, &display);
        g_app_rtos_stats.app_stack_free_words = uxTaskGetStackHighWaterMark(NULL);
        g_app_rtos_stats.heap_free_bytes = xPortGetFreeHeapSize();
        g_app_rtos_stats.heap_min_free_bytes = xPortGetMinimumEverFreeHeapSize();
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}

static void Task_UI_Feedback(void *argument)
{
    DisplayEvent_t current = {.status = APP_READY};
    DisplayEvent_t next;
    uint8_t lcd_ready = 0, dirty = 1, showing_result = 0;
    uint8_t have_system = 0;
    uint32_t shown_at = HAL_GetTick();
    uint32_t last_init = HAL_GetTick() - 2000U;
    (void)argument;
    for (;;) {
        Feedback_Process();
        uint32_t now = HAL_GetTick();
        if (!Feedback_IsBusy()) {
            if (showing_result && (uint32_t)(now - shown_at) >= RESULT_HOLD_MS)
                showing_result = 0;
            if (!showing_result) {
                if (xQueueReceive(g_display_event_queue, &next, 0) == pdPASS) {
                    current = next;
                    dirty = 1;
                    showing_result = 1;

                    if (lcd_ready && LCD_ShowEvent(&current) != LCD_OK) {
                        lcd_ready = 0;
                        ++g_app_rtos_stats.lcd_errors;
                    }
                    if (lcd_ready) dirty = 0;
                    (void)Feedback_Start(current.status);
                    shown_at = HAL_GetTick();
                } else if (xQueuePeek(g_system_display_queue, &next, 0) == pdPASS) {
                    if (!have_system || current.status != next.status) {
                        current = next;
                        dirty = 1;
                    }
                    have_system = 1;
                }
            }

            if (!Feedback_IsBusy()) {
                now = HAL_GetTick();
                if (!lcd_ready && (uint32_t)(now - last_init) >= 2000U) {
                    lcd_ready = LCD_Init() == LCD_OK;
                    last_init = HAL_GetTick();
                    dirty = 1;
                    if (!lcd_ready) ++g_app_rtos_stats.lcd_errors;
                }
                if (lcd_ready && dirty && (have_system || showing_result)) {
                    if (LCD_ShowEvent(&current) == LCD_OK) dirty = 0;
                    else { lcd_ready = 0; ++g_app_rtos_stats.lcd_errors; }
                }
            }
        }
        g_app_rtos_stats.ui_stack_free_words = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}

void AppRTOS_Start(void)
{
    configASSERT(App_Queues_Init() == pdPASS);
    configASSERT(RFID_Task_Start() == pdPASS);
    configASSERT(xTaskCreate(Task_Storage_App, "Storage_App", APP_STACK_WORDS,
                            NULL, 1U, NULL) == pdPASS);
    configASSERT(xTaskCreate(Task_UI_Feedback, "UI_Feedback", UI_STACK_WORDS,
                            NULL, 2U, NULL) == pdPASS);
    vTaskStartScheduler();
    app_assert_failed("scheduler failed to start", 0);
}
