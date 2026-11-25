#ifndef APP_RTOS_H
#define APP_RTOS_H
#include <stdint.h>

void AppRTOS_Start(void);

typedef struct {
    uint32_t input_events;
    uint32_t accepted;
    uint32_t duplicates;
    uint32_t unknown;
    uint32_t storage_failures;
    uint32_t display_waits;
    uint32_t lcd_errors;
    uint32_t app_stack_free_words;
    uint32_t ui_stack_free_words;
    uint32_t heap_free_bytes;
    uint32_t heap_min_free_bytes;
} AppRTOS_Stats_t;
extern volatile AppRTOS_Stats_t g_app_rtos_stats;
extern const char * volatile g_rtos_fault_file;
extern volatile int g_rtos_fault_line;
#endif
