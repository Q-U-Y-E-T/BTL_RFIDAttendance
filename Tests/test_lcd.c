/*
 * test_lcd.c - Test thu cong module LCD tren board.
 *
 * KHONG nam trong build mac dinh. Cach bat: xem docs/LCD.md, muc "Chay test".
 *
 * Test_LCD_Run() chay lan luot cac buoc, moi buoc giu man hinh
 * TEST_LCD_STEP_MS de nguoi test nhin va doi chieu voi bang ky vong
 * trong docs/LCD.md. Ham tra ve so buoc that bai (0 = dat).
 */
#include "lcd.h"
#include "main.h"

#include <string.h>

#define TEST_LCD_STEP_MS    2500U

/* Doc duoc tu debugger: buoc that bai dau tien (0 = chua co loi). */
volatile uint32_t g_test_lcd_first_fail_step;

static uint32_t s_fail_count;
static uint32_t s_step;

static void test_check(int ok)
{
    s_step++;
    if (!ok) {
        s_fail_count++;
        if (g_test_lcd_first_fail_step == 0U) {
            g_test_lcd_first_fail_step = s_step;
        }
    }
}

static void test_make_event(DisplayEvent_t *ev, AppStatus_t status,
                            const char *id, const char *name)
{
    memset(ev, 0, sizeof(*ev));
    ev->status = status;
    strncpy(ev->student_id, id, sizeof(ev->student_id) - 1U);
    strncpy(ev->student_name, name, sizeof(ev->student_name) - 1U);
}

/* Hien thi, kiem tra tra ve LCD_OK va du lieu goc khong bi sua. */
static void test_show(AppStatus_t status, const char *id, const char *name)
{
    DisplayEvent_t ev;
    DisplayEvent_t copy;

    test_make_event(&ev, status, id, name);
    copy = ev;

    test_check(LCD_ShowEvent(&ev) == LCD_OK &&
               memcmp(&ev, &copy, sizeof(ev)) == 0);
    HAL_Delay(TEST_LCD_STEP_MS);
}

uint32_t Test_LCD_Run(void)
{
    DisplayEvent_t bad;

    s_fail_count = 0U;
    s_step = 0U;
    g_test_lcd_first_fail_step = 0U;

    /* 1. Khoi tao */
    test_check(LCD_Init() == LCD_OK);

    /* 2. Con tro NULL bi tu choi */
    test_check(LCD_ShowEvent(NULL) == LCD_ERROR);

    /* 3..7. Du 5 trang thai */
    test_show(APP_READY, "", "");
    test_show(APP_CARD_ACCEPTED, "HS001", "NGUYEN VAN A");
    test_show(APP_CARD_UNKNOWN, "", "");
    test_show(APP_STORAGE_ERROR, "", "");
    test_show(APP_LIST_ERROR, "", "");

    /* 8. Ma 15 ky tu va ten 48 ky tu: ten bi cat con 16 ky tu */
    test_show(APP_CARD_ACCEPTED, "B23DCDT225ABCDE",
              "TRAN NGOC SON TEN RAT DAI DE KIEM TRA CAT CHUOI X");

    /* 9. Noi dung ngan sau noi dung dai: khong con chu cu */
    test_show(APP_CARD_ACCEPTED, "HS2", "AN");

    /* 10. Trang thai khong hop le bi tu choi, man hinh giu "HS2 / AN" */
    test_make_event(&bad, (AppStatus_t)99, "", "");
    test_check(LCD_ShowEvent(&bad) == LCD_ERROR);
    HAL_Delay(TEST_LCD_STEP_MS);

    /* 11. Ve lai man hinh cho */
    test_show(APP_READY, "", "");

    return s_fail_count;
}
