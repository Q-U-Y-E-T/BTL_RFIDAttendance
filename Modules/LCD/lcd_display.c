/*
 * lcd_display.c - Quan ly hien thi LCD cho vong lap chinh (Tran Ngoc Son).
 * Xem lcd_display.h.
 */
#include "lcd_display.h"

#include "lcd.h"
#include "i2c.h"

#include <stdbool.h>
#include <stddef.h>

volatile Display_Stats_t g_display_stats;

static DisplayEvent_t s_current;     /* Noi dung can hien tren LCD         */
static uint32_t s_shown_at;          /* Thoi diem nhan s_current            */
static uint32_t s_last_attempt;      /* Lan thu ghi LCD gan nhat           */
static bool s_need_write;            /* s_current chua len man hinh        */
static bool s_had_failure;           /* Dang trong chuoi loi               */

static bool status_valid(AppStatus_t status)
{
    switch (status) {
    case APP_READY:
    case APP_CARD_ACCEPTED:
    case APP_CARD_UNKNOWN:
    case APP_STORAGE_ERROR:
    case APP_LIST_ERROR:
        return true;
    default:
        return false;
    }
}

static void set_ready(void)
{
    s_current.status = APP_READY;
    s_current.student_id[0] = '\0';
    s_current.student_name[0] = '\0';
}

/* Ghi s_current len LCD; neu loi thi reset I2C1, khoi tao lai, thu them 1 lan. */
static void write_current(uint32_t now_ms)
{
    s_last_attempt = now_ms;

    if (g_display_stats.lcd_ok != 0U) {
        if (LCD_ShowEvent(&s_current) == LCD_OK) {
            ++g_display_stats.screens_written;
            s_need_write = false;
            return;
        }
        ++g_display_stats.write_errors;
        s_had_failure = true;
    }

    if (s_had_failure) {
        /* Go loi ket co BUSY cua I2C STM32F1 (HAL_I2C_Init co SWRST). */
        (void)HAL_I2C_DeInit(&hi2c1);
        (void)HAL_I2C_Init(&hi2c1);
        ++g_display_stats.bus_resets;
    }

    if (LCD_Init() != LCD_OK) {
        ++g_display_stats.init_errors;
        g_display_stats.lcd_ok = 0U;
        s_had_failure = true;
        s_need_write = true;
        return;
    }
    if (LCD_ShowEvent(&s_current) != LCD_OK) {
        ++g_display_stats.write_errors;
        g_display_stats.lcd_ok = 0U;
        s_had_failure = true;
        s_need_write = true;
        return;
    }

    if (s_had_failure) {
        ++g_display_stats.recoveries;
        s_had_failure = false;
    }
    g_display_stats.lcd_ok = 1U;
    ++g_display_stats.screens_written;
    s_need_write = false;
}

void Display_Init(uint32_t now_ms)
{
    set_ready();
    s_shown_at = now_ms;
    s_had_failure = false;
    g_display_stats.lcd_ok = 0U;
    write_current(now_ms);
}

void Display_Show(const DisplayEvent_t *event, uint32_t now_ms)
{
    if (event == NULL || !status_valid(event->status)) {
        ++g_display_stats.rejected;
        return;
    }
    s_current = *event;
    s_current.student_id[sizeof(s_current.student_id) - 1U] = '\0';
    s_current.student_name[sizeof(s_current.student_name) - 1U] = '\0';
    s_shown_at = now_ms;
    ++g_display_stats.events_shown;
    write_current(now_ms);
}

void Display_Process(uint32_t now_ms)
{
    /*
     * Moi ket qua (hop le, the la, loi) la phan hoi cho MOT lan quet:
     * giu LCD_DISPLAY_HOLD_MS roi ve man hinh cho.
     */
    if (s_current.status != APP_READY &&
        (uint32_t)(now_ms - s_shown_at) >= LCD_DISPLAY_HOLD_MS) {
        set_ready();
        s_shown_at = now_ms;
        s_need_write = true;
        ++g_display_stats.returns_to_ready;
        write_current(now_ms);
        return;
    }

    if (s_need_write &&
        (uint32_t)(now_ms - s_last_attempt) >= LCD_DISPLAY_RETRY_MS) {
        write_current(now_ms);
    }
}
