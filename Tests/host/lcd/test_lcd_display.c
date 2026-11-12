/*
 * Kiem thu phan LCD cua Tran Ngoc Son tren may tinh (khong can board).
 *
 * - LCD 1602 + PCF8574 duoc gia lap: giai ma tung byte I2C nhu chip that,
 *   nen doc lai duoc dung chu dang hien tren 2 dong.
 * - Phan A: Modules/LCD/lcd_display.c + lcd.c (code that).
 * - Phan B: ghep voi App cua Quyet (app.c, attendance.c, storage, FatFs that
 *   tren dia RAM), mo phong dung vong lap trong Core/Src/main.c.
 *
 * Chay: make test-host   (hoac python3 Tests/run_lcd_host_tests.py)
 */
#include "main.h"
#include "ff.h"
#include "diskio.h"
#include "sd_diskio.h"
#include "storage.h"
#include "storage_manager.h"
#include "feedback.h"
#include "app.h"
#include "lcd.h"
#include "lcd_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_pass;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "LOI %s:%d: %s\n", __FILE__, __LINE__, #c); abort(); \
} } while (0)
#define CASE(name) do { ++g_pass; printf("PASS %02u %s\n", g_pass, name); } while (0)

/* ---------------------------------------------------------------------------
 * Thoi gian va GPIO
 * ------------------------------------------------------------------------- */
static uint32_t tick;
static GPIO_PinState led, buzzer;
uint32_t HAL_GetTick(void) { return tick; }
void HAL_Delay(uint32_t ms) { tick += ms; }
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state)
{
    (void)pin;
    if (port == LED_GPIO_Port) led = state;
    else if (port == BUZZER_GPIO_Port) buzzer = state;
}

/* ---------------------------------------------------------------------------
 * LCD 1602 + PCF8574 gia lap
 * ------------------------------------------------------------------------- */
I2C_HandleTypeDef hi2c1;
static int lcd_present = 1, bus_stuck;
static unsigned i2c_bytes, i2c_inits, i2c_deinits;
static uint8_t prev, hi_nib, ac;
static int mode8 = 1, have_hi, disp_on;
static char ddram[128];

static void lcd_power_on(void)
{
    mode8 = 1; have_hi = 0; prev = 0; ac = 0; disp_on = 0;
    memset(ddram, '#', sizeof ddram);
}
static void lcd_exec(uint8_t v, int rs)
{
    if (rs) { ddram[ac & 0x7F] = (char)v; ac++; return; }
    if (v == 0x01) { memset(ddram, ' ', sizeof ddram); ac = 0; }
    else if (v & 0x80) ac = v & 0x7F;
    else if ((v & 0xF0) == 0x30) mode8 = 1;
    else if ((v & 0xF8) == 0x08) disp_on = (v & 0x04) != 0;
}
static void lcd_latch(uint8_t b)
{
    uint8_t nib = b & 0xF0;
    if (mode8) { if (nib == 0x20) { mode8 = 0; have_hi = 0; } return; }
    if (!have_hi) { hi_nib = nib; have_hi = 1; }
    else { have_hi = 0; lcd_exec(hi_nib | (nib >> 4), b & 1); }
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr,
                                          uint8_t *p, uint16_t n, uint32_t to)
{
    uint16_t i;
    (void)h; (void)to;
    if (bus_stuck || !lcd_present || addr != (0x27 << 1)) return HAL_ERROR;
    for (i = 0; i < n; i++) {
        if ((prev & 0x04) && !(p[i] & 0x04)) lcd_latch(p[i]);
        prev = p[i];
    }
    i2c_bytes += n;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr,
                                        uint32_t t, uint32_t to)
{
    (void)h; (void)t; (void)to;
    return (!bus_stuck && lcd_present && addr == (0x27 << 1)) ? HAL_OK : HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h) { (void)h; ++i2c_deinits; return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h) { (void)h; ++i2c_inits; bus_stuck = 0; return HAL_OK; }

static int screen_is(const char *l1, const char *l2)
{
    char a[17], b[17];
    memcpy(a, ddram, 16); a[16] = 0;
    memcpy(b, ddram + 0x40, 16); b[16] = 0;
    if (disp_on && !strcmp(a, l1) && !strcmp(b, l2)) return 1;
    fprintf(stderr, "  LCD dang hien |%s|%s|, ky vong |%s|%s|\n", a, b, l1, l2);
    return 0;
}
#define READY "MOI QUET THE    ", "                "

static DisplayEvent_t ev(AppStatus_t st, const char *id, const char *name)
{
    DisplayEvent_t e;
    memset(&e, 0, sizeof e);
    e.status = st;
    strncpy(e.student_id, id, sizeof e.student_id - 1);
    strncpy(e.student_name, name, sizeof e.student_name - 1);
    return e;
}

static void run_display_ms(uint32_t ms)
{
    uint32_t end = tick + ms;
    while ((int32_t)(end - tick) > 0) { Display_Process(tick); tick += 5; }
}

/* ---------------------------------------------------------------------------
 * Phan A: lcd_display.c
 * ------------------------------------------------------------------------- */
static void test_display(void)
{
    DisplayEvent_t e;
    unsigned before, attempts_start;

    lcd_power_on(); lcd_present = 1; bus_stuck = 0; tick = 1000;
    memset((void *)&g_display_stats, 0, sizeof g_display_stats);
    Display_Init(tick);
    CHECK(screen_is(READY));
    CASE("Display_Init hien MOI QUET THE");

    e = ev(APP_CARD_ACCEPTED, "SV001", "NGUYEN VAN A");
    Display_Show(&e, tick);
    CHECK(screen_is("SV001           ", "NGUYEN VAN A    "));
    run_display_ms(2990);
    CHECK(screen_is("SV001           ", "NGUYEN VAN A    "));
    run_display_ms(20);
    CHECK(screen_is(READY));
    CASE("the hop le: hien ngay, giu 3 s roi ve MOI QUET THE");

    e = ev(APP_CARD_UNKNOWN, "", "");
    Display_Show(&e, tick);
    CHECK(screen_is("THE KHONG HOP LE", "VUI LONG THU LAI"));
    run_display_ms(3010);
    CHECK(screen_is(READY));
    CASE("the la: THE KHONG HOP LE 3 s");

    e = ev(APP_STORAGE_ERROR, "", "");
    Display_Show(&e, tick);
    CHECK(screen_is("LOI LUU DU LIEU ", "KIEM TRA THE SD "));
    run_display_ms(3010);
    e = ev(APP_LIST_ERROR, "", "");
    Display_Show(&e, tick);
    CHECK(screen_is("LOI DANH SACH   ", "KIEM TRA FILE   "));
    run_display_ms(3010);
    CHECK(screen_is(READY));
    CASE("loi luu / loi danh sach hien 3 s cho moi lan quet");

    e = ev(APP_CARD_ACCEPTED, "B23DCDT225ABCDE", "TRAN NGOC SON TEN RAT DAI DE KIEM TRA CAT CHUOI");
    Display_Show(&e, tick);
    CHECK(screen_is("B23DCDT225ABCDE ", "TRAN NGOC SON TE"));
    run_display_ms(1000);
    e = ev(APP_CARD_ACCEPTED, "SV2", "AN");
    Display_Show(&e, tick);
    CHECK(screen_is("SV2             ", "AN              "));
    run_display_ms(2500);
    CHECK(screen_is("SV2             ", "AN              "));
    run_display_ms(510);
    CHECK(screen_is(READY));
    CASE("ten dai bi cat; ket qua moi thay ngay, khong sot chu, tinh lai 3 s");

    before = g_display_stats.rejected;
    Display_Show(NULL, tick);
    e = ev((AppStatus_t)99, "X", "Y");
    Display_Show(&e, tick);
    CHECK(g_display_stats.rejected == before + 2);
    CHECK(screen_is(READY));
    CASE("NULL va status sai bi bo qua, man hinh giu nguyen");

    /* Chuoi thieu '\0' (day ky tu) khong lam doc tran */
    memset(&e, 'Z', sizeof e);
    e.status = APP_CARD_ACCEPTED;
    Display_Show(&e, tick);
    /* Ma 16 byte khong co '\0': Display_Show tu dat '\0' o byte cuoi -> 15 ky tu */
    CHECK(screen_is("ZZZZZZZZZZZZZZZ ", "ZZZZZZZZZZZZZZZZ"));
    run_display_ms(3010);
    CASE("chuoi thieu '\\0' van an toan");

    before = i2c_bytes;
    run_display_ms(5000);
    CHECK(i2c_bytes == before);
    CASE("dung yen: khong co giao dich I2C thua");

    /* Rut LCD (kem I2C ket BUSY), quet the, cam lai */
    lcd_present = 0; bus_stuck = 1;
    e = ev(APP_CARD_ACCEPTED, "SV001", "NGUYEN VAN A");
    Display_Show(&e, tick);
    CHECK(g_display_stats.lcd_ok == 0);
    attempts_start = g_display_stats.init_errors;
    run_display_ms(1000);
    CHECK(g_display_stats.init_errors - attempts_start >= 1 &&
          g_display_stats.init_errors - attempts_start <= 3);   /* ~0.5 s/lan */
    CASE("LCD mat: dem loi, thu lai cach 0.5 s, khong treo vong lap");

    lcd_power_on(); lcd_present = 1;            /* bus_stuck van = 1 */
    run_display_ms(600);
    CHECK(screen_is("SV001           ", "NGUYEN VAN A    "));
    CHECK(g_display_stats.recoveries == 1);
    CHECK(i2c_inits >= 1 && i2c_inits == i2c_deinits);
    run_display_ms(2000);
    CHECK(screen_is(READY));
    CASE("cam lai: reset I2C1, khoi tao lai, hien dung noi dung roi ve cho");
}

/* ---------------------------------------------------------------------------
 * Phan B: ghep voi App cua Quyet (FatFs that tren dia RAM)
 * Dia gia lap lay theo Tests/host/test_suite.c cua Quyet.
 * ------------------------------------------------------------------------- */
#define SECTORS 8192U
static BYTE image[SECTORS * 512U];
static uint8_t present;
static DSTATUS disk_state;

DSTATUS disk_status(BYTE pdrv) { return pdrv == 0 && present ? disk_state : STA_NOINIT; }
DSTATUS disk_initialize(BYTE pdrv) { disk_state = pdrv == 0 && present ? 0 : STA_NOINIT; return disk_state; }
uint8_t SD_DiskTryReset(void) { disk_state = STA_NOINIT; return present; }
uint8_t SD_DiskProbe(void) { if (!present) disk_state = STA_NOINIT; return present && !disk_state; }
DRESULT disk_read(BYTE pdrv, BYTE *buf, DWORD sector, UINT count)
{
    if (pdrv || !buf || !count || sector >= SECTORS || count > SECTORS - sector) return RES_PARERR;
    if (!present || disk_state) return RES_NOTRDY;
    memcpy(buf, image + sector * 512U, count * 512U); return RES_OK;
}
DRESULT disk_write(BYTE pdrv, const BYTE *buf, DWORD sector, UINT count)
{
    if (pdrv || !buf || !count || sector >= SECTORS || count > SECTORS - sector) return RES_PARERR;
    if (!present || disk_state) return RES_NOTRDY;
    memcpy(image + sector * 512U, buf, count * 512U); return RES_OK;
}
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf)
{
    if (pdrv || !present || disk_state) return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC: return RES_OK;
    case GET_SECTOR_COUNT: *(DWORD *)buf = SECTORS; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)buf = 512; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)buf = 1; return RES_OK;
    default: return RES_PARERR;
    }
}
static void put16(size_t at, unsigned v) { image[at] = (BYTE)v; image[at + 1] = (BYTE)(v >> 8); }
static void format_disk(void)
{
    memset(image, 0, sizeof image);
    image[0] = 0xEB; image[1] = 0x3C; image[2] = 0x90;
    memcpy(image + 3, "RFIDTEST", 8);
    put16(11, 512); image[13] = 1; put16(14, 1); image[16] = 2;
    put16(17, 512); put16(19, SECTORS); image[21] = 0xF8;
    put16(22, 32); put16(24, 32); put16(26, 64);
    image[36] = 0x80; image[38] = 0x29;
    memcpy(image + 43, "RFID TEST  ", 11); memcpy(image + 54, "FAT16   ", 8);
    image[510] = 0x55; image[511] = 0xAA;
    put16(512, 0xFFF8); put16(514, 0xFFFF);
    put16(33U * 512U, 0xFFF8); put16(33U * 512U + 2, 0xFFFF);
    present = 1; disk_state = STA_NOINIT;
}
static void write_csv(void)
{
    static const char csv[] =
        "uid,student_id,student_name\r\n"
        "B369A2A7,SV001,NGUYEN VAN A\r\n"
        "04112233445566,SV002,TRAN THI B\r\n";
    FIL f; UINT n;
    CHECK(Storage_Init() == STORAGE_OK);
    CHECK(f_open(&f, "0:/DanhSach.csv", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK);
    CHECK(f_write(&f, csv, sizeof csv - 1, &n) == FR_OK && n == sizeof csv - 1);
    CHECK(f_close(&f) == FR_OK);
}

/* Mot vong lap giong Core/Src/main.c (USER CODE 3) */
static void main_loop_once(void)
{
    DisplayEvent_t display;
    App_Process();
    if (App_TakeDisplayEvent(&display)) {
        Display_Show(&display, HAL_GetTick());
    }
    Display_Process(HAL_GetTick());
    HAL_Delay(5);
}
static void run_main_ms(uint32_t ms)
{
    uint32_t end = tick + ms;
    while ((int32_t)(end - tick) > 0) main_loop_once();
}
static AttendanceResult_t scan(const uint8_t *uid, uint8_t len)
{
    CardEvent_t c;
    memset(&c, 0, sizeof c);
    memcpy(c.uid, uid, len); c.uid_len = len; c.uptime_ms = tick;
    return App_SubmitCard(&c);
}
static void card_removed(uint32_t ms)
{
    uint32_t end = tick + ms;
    while ((int32_t)(end - tick) > 0) { App_NotifyNoCard(tick); main_loop_once(); }
}

static void test_with_app(void)
{
    static const uint8_t sv001[] = { 0xB3, 0x69, 0xA2, 0xA7 };
    static const uint8_t sv002[] = { 0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    static const uint8_t la[]    = { 0xDE, 0xAD, 0xBE, 0xEF };
    AttendanceResult_t r;

    /* Bat nguon nhu main.c: App_Init roi Display_Init */
    format_disk(); write_csv(); CHECK(f_mount(NULL, "0:", 0) == FR_OK);
    lcd_power_on(); lcd_present = 1; bus_stuck = 0; tick = 0;
    App_Init();
    Display_Init(HAL_GetTick());
    CHECK(screen_is(READY));
    run_main_ms(2000);                          /* SD nap danh sach */
    CHECK(StorageManager_IsReady());
    CHECK(screen_is(READY));
    CASE("ghep App: bat nguon hien MOI QUET THE, SD nap danh sach");

    r = scan(sv001, 4);
    CHECK(r == ATTENDANCE_ACCEPTED);
    main_loop_once();
    CHECK(screen_is("SV001           ", "NGUYEN VAN A    "));
    CASE("ghep App: quet SV001 -> LCD hien ma + ten ngay vong lap ke tiep");

    run_main_ms(1000);
    CHECK(scan(sv001, 4) == ATTENDANCE_DUPLICATE);     /* van giu the */
    main_loop_once();
    CHECK(screen_is("SV001           ", "NGUYEN VAN A    "));
    run_main_ms(2100);
    CHECK(screen_is(READY));
    CASE("ghep App: giu the khong hien lai; 3 s ve MOI QUET THE");

    card_removed(800);
    r = scan(la, 4);
    CHECK(r == ATTENDANCE_UNKNOWN);
    main_loop_once();
    CHECK(screen_is("THE KHONG HOP LE", "VUI LONG THU LAI"));
    CASE("ghep App: the la -> THE KHONG HOP LE");

    run_main_ms(1500);                          /* het mau LED/coi */
    r = scan(sv002, 7);
    CHECK(r == ATTENDANCE_ACCEPTED);
    main_loop_once();
    CHECK(screen_is("SV002           ", "TRAN THI B      "));
    CASE("ghep App: the khac (UID 7 byte) thay ngay");

    /* LCD hong: App van nhan luot quet tiep theo (khong ket BUSY) */
    run_main_ms(3100);
    card_removed(800);
    lcd_present = 0; bus_stuck = 1;
    CHECK(scan(sv001, 4) == ATTENDANCE_ACCEPTED);
    run_main_ms(1500);
    card_removed(800);
    r = scan(la, 4);
    CHECK(r == ATTENDANCE_UNKNOWN);
    CASE("ghep App: LCD hong van lay ket qua, App khong bi ket BUSY");

    lcd_power_on(); lcd_present = 1;
    run_main_ms(600);
    CHECK(screen_is("THE KHONG HOP LE", "VUI LONG THU LAI"));
    run_main_ms(3100);
    CHECK(screen_is(READY));
    CASE("ghep App: cam lai LCD -> hien ket qua moi nhat roi ve cho");

    /* Rut the SD: quet -> bao loi tren LCD */
    run_main_ms(1500);
    card_removed(800);
    present = 0;
    run_main_ms(2500);
    r = scan(sv002, 7);
    CHECK(r == ATTENDANCE_NOT_READY || r == ATTENDANCE_STORAGE_FAILED);
    main_loop_once();
    CHECK(screen_is("LOI LUU DU LIEU ", "KIEM TRA THE SD ") ||
          screen_is("LOI DANH SACH   ", "KIEM TRA FILE   "));
    CASE("ghep App: rut the SD -> LCD bao loi");
}

int main(void)
{
    test_display();
    test_with_app();
    printf("ALL %u LCD CASES PASSED\n", g_pass);
    return 0;
}
