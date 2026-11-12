/*
 * lcd_display.h - Quan ly hien thi LCD cho vong lap chinh (Tran Ngoc Son).
 *
 * Nhan DisplayEvent_t do App_TakeDisplayEvent() tra ve, hien len LCD 16x2
 * qua driver lcd.c, giu ket qua LCD_DISPLAY_HOLD_MS roi tu ve "MOI QUET THE".
 * LCD loi (rut day, nhieu I2C): thu lai moi LCD_DISPLAY_RETRY_MS, co reset
 * ngoai vi I2C1, khong lam treo vong lap.
 *
 * Cach dung trong main.c (cung vong lap voi App):
 *
 *   App_Init();
 *   Display_Init(HAL_GetTick());
 *   while (1) {
 *       DisplayEvent_t display;
 *       App_Process();
 *       if (App_TakeDisplayEvent(&display)) {
 *           Display_Show(&display, HAL_GetTick());
 *       }
 *       Display_Process(HAL_GetTick());
 *       HAL_Delay(5);
 *   }
 *
 * Chi goi cac ham nay tu mot vong lap/task (noi duy nhat dung LCD va I2C1).
 */
#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <stdint.h>
#include "app_types.h"

#ifndef LCD_DISPLAY_HOLD_MS
#define LCD_DISPLAY_HOLD_MS   3000U   /* Giu ket qua 3 s roi ve MOI QUET THE */
#endif
#ifndef LCD_DISPLAY_RETRY_MS
#define LCD_DISPLAY_RETRY_MS  500U    /* LCD loi: thu lai sau 0.5 s          */
#endif

typedef struct {
    uint32_t events_shown;      /* So DisplayEvent_t nhan qua Display_Show  */
    uint32_t screens_written;   /* So lan ghi man hinh thanh cong           */
    uint32_t returns_to_ready;  /* So lan tu ve MOI QUET THE                */
    uint32_t init_errors;       /* LCD_Init that bai                        */
    uint32_t write_errors;      /* LCD_ShowEvent that bai                   */
    uint32_t recoveries;        /* Phuc hoi LCD thanh cong sau loi          */
    uint32_t bus_resets;        /* So lan reset ngoai vi I2C1               */
    uint32_t rejected;          /* Display_Show voi NULL hoac status sai    */
    uint8_t  lcd_ok;            /* 1 = LCD dang hien dung noi dung          */
} Display_Stats_t;

/* Xem bang debugger. */
extern volatile Display_Stats_t g_display_stats;

/* Khoi tao LCD va hien MOI QUET THE. Goi sau MX_I2C1_Init(). */
void Display_Init(uint32_t now_ms);

/*
 * Hien ngay mot ket qua (sao chep *event). Ket qua moi thay ket qua cu
 * va tinh lai moc 3 s. NULL hoac status sai: bo qua, man hinh giu nguyen.
 */
void Display_Show(const DisplayEvent_t *event, uint32_t now_ms);

/* Goi thuong xuyen: tu ve MOI QUET THE va thu lai khi LCD loi. */
void Display_Process(uint32_t now_ms);

#endif /* LCD_DISPLAY_H */
