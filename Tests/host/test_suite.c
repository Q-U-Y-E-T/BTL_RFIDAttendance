/* Tests production Storage + actual FatFs R0.11 on a RAM FAT16 disk.
 * Only disk hardware, HAL time and GPIO are substituted.
 */
#include "main.h"
#include "ff.h"
#include "diskio.h"
#include "sd_diskio.h"
#include "storage.h"
#include "storage_manager.h"
#include "attendance.h"
#include "feedback.h"
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTORS 8192U
static BYTE image[SECTORS * 512U];
static uint32_t tick;
static uint8_t present;
static DSTATUS disk_state;
static unsigned write_calls;
static unsigned read_calls;
static int fail_write;
static int fail_read;
static GPIO_PinState led, buzzer;
static unsigned tests;

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); abort(); \
} } while (0)
#define CASE(name) do { ++tests; printf("PASS %02u %s\n", tests, name); } while (0)

uint32_t HAL_GetTick(void) { return tick; }
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state)
{
    (void)pin;
    if (port == LED_GPIO_Port) led = state;
    else if (port == BUZZER_GPIO_Port) buzzer = state;
}
DSTATUS disk_status(BYTE pdrv)
{ return pdrv == 0 && present ? disk_state : STA_NOINIT; }
DSTATUS disk_initialize(BYTE pdrv)
{ disk_state = pdrv == 0 && present ? 0 : STA_NOINIT; return disk_state; }
uint8_t SD_DiskTryReset(void)
{ disk_state = STA_NOINIT; return present; }
uint8_t SD_DiskProbe(void)
{ if (!present) disk_state = STA_NOINIT; return present && !disk_state; }
DRESULT disk_read(BYTE pdrv, BYTE *buf, DWORD sector, UINT count)
{
    ++read_calls;
    if (pdrv || !buf || !count || sector >= SECTORS || count > SECTORS-sector)
        return RES_PARERR;
    if (!present || disk_state) return RES_NOTRDY;
    if (fail_read) return RES_ERROR;
    memcpy(buf, image + sector*512U, count*512U); return RES_OK;
}
DRESULT disk_write(BYTE pdrv, const BYTE *buf, DWORD sector, UINT count)
{
    ++write_calls;
    if (pdrv || !buf || !count || sector >= SECTORS || count > SECTORS-sector)
        return RES_PARERR;
    if (!present || disk_state) return RES_NOTRDY;
    if (fail_write) return RES_ERROR;
    memcpy(image + sector*512U, buf, count*512U); return RES_OK;
}
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf)
{
    if (pdrv || !present || disk_state) return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC: return fail_write ? RES_ERROR : RES_OK;
    case GET_SECTOR_COUNT: if (!buf) return RES_PARERR; *(DWORD *)buf=SECTORS; return RES_OK;
    case GET_SECTOR_SIZE: if (!buf) return RES_PARERR; *(WORD *)buf=512; return RES_OK;
    case GET_BLOCK_SIZE: if (!buf) return RES_PARERR; *(DWORD *)buf=1; return RES_OK;
    default: return RES_PARERR;
    }
}
static void put16(size_t at, unsigned v)
{ image[at]=(BYTE)v; image[at+1]=(BYTE)(v>>8); }
static void reset_disk(void)
{
    CHECK(f_mount(NULL,"0:",0)==FR_OK);
    memset(image,0,sizeof image);
    image[0]=0xEB; image[1]=0x3C; image[2]=0x90;
    memcpy(image+3,"RFIDTEST",8);
    put16(11,512); image[13]=1; put16(14,1); image[16]=2;
    put16(17,512); put16(19,SECTORS); image[21]=0xF8;
    put16(22,32); put16(24,32); put16(26,64);
    image[36]=0x80; image[38]=0x29;
    memcpy(image+43,"RFID TEST  ",11); memcpy(image+54,"FAT16   ",8);
    image[510]=0x55; image[511]=0xAA;
    put16(512,0xFFF8); put16(514,0xFFFF);
    put16(33U*512U,0xFFF8); put16(33U*512U+2,0xFFFF);
    present=1; disk_state=STA_NOINIT; fail_write=fail_read=0;
    write_calls=read_calls=0; tick=0;
    CHECK(Storage_Init()==STORAGE_OK);
}
static void write_file(const char *name, const void *data, size_t len)
{
    FIL f; UINT n;
    CHECK(f_open(&f,name,FA_WRITE|FA_CREATE_ALWAYS)==FR_OK);
    CHECK(f_write(&f,data,(UINT)len,&n)==FR_OK && n==len);
    CHECK(f_close(&f)==FR_OK);
}
static size_t read_file(const char *name, char *data, size_t capacity)
{
    FIL f; UINT n;
    CHECK(f_open(&f,name,FA_READ)==FR_OK);
    CHECK(f_read(&f,data,(UINT)(capacity-1),&n)==FR_OK);
    CHECK(f_close(&f)==FR_OK); data[n]=0; return n;
}
static const char good_csv[]=
    "uid,student_id,student_name\r\n"
    "00112233,SV001,NGUYEN VAN A\r\n"
    "04112233445566,SV002,TRAN THI B\r\n"
    "00112233445566778899,SV003,LE VAN C\r\n";
static void seed(void)
{ write_file("0:/DanhSach.csv",good_csv,sizeof good_csv-1); }
static void absent_stable(void)
{ for (unsigned i=0;i<8;++i) { tick+=100; App_NotifyNoCard(tick); } }
static void feedback_done(void)
{ for (unsigned i=0;i<8;++i) { tick+=250; Feedback_Process(); } }

static void test_csv(void)
{
    Student_t list[50]; uint16_t n=99;
    reset_disk();
    CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_FILE_NOT_FOUND && n==0);
    CASE("missing CSV returns error and count=0");
    seed(); CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_OK && n==3);
    CHECK(list[0].uid_len==4 && list[0].uid[0]==0);
    CHECK(list[1].uid_len==7 && list[2].uid_len==10);
    CHECK(!strcmp(list[0].student_id,"SV001"));
    CASE("real FatFs reads CSV, 4/7/10-byte UIDs and leading zero");
    CHECK(Storage_LoadStudents(list,2,&n)==STORAGE_CAPACITY_ERROR && n==0);
    CHECK(Storage_LoadStudents(NULL,50,&n)==STORAGE_INVALID_ARGUMENT && n==0);
    CHECK(Storage_LoadStudents(list,0,&n)==STORAGE_INVALID_ARGUMENT && n==0);
    CHECK(Storage_LoadStudents(list,51,&n)==STORAGE_INVALID_ARGUMENT && n==0);
    CHECK(Storage_LoadStudents(list,50,NULL)==STORAGE_INVALID_ARGUMENT);
    CASE("capacity and invalid arguments");
    const char *bad[]={
        "UID,student_id,student_name\n00112233,S1,NAME\n",
        "uid,student_id,student_name\n00112233,S1,A\n00112233,S2,B\n",
        "uid,student_id,student_name\n00XX2233,S1,A\n",
        "uid,student_id,student_name\n001122,S1,A\n",
        "uid,student_id,student_name\n00112233,,A\n",
        "uid,student_id,student_name\n00112233,S1,\n",
        "uid,student_id,student_name\n00112233,S1,   \n",
        "uid,student_id,student_name\n00112233,1234567890123456,A\n",
        "uid,student_id,student_name\n00112233,S1,A,B\n",
        "uid,student_id,student_name\n\n",
        "uid,student_id,student_name\n00112233,S1,\"NAME\"\n"
    };
    for (size_t i=0;i<sizeof bad/sizeof bad[0];++i) {
        write_file("0:/DanhSach.csv",bad[i],strlen(bad[i])); n=99;
        CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_FORMAT_ERROR && n==0);
    }
    CASE("11 malformed CSV cases rejected");
    char large[7000]; size_t used=(size_t)sprintf(large,"uid,student_id,student_name\n");
    for (unsigned i=0;i<51;++i) used+=(size_t)sprintf(large+used,"%08X,S%u,NAME\n",i,i);
    write_file("0:/DanhSach.csv",large,used);
    CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_CAPACITY_ERROR && n==0);
    CASE("51 students rejected without truncation");
    used=(size_t)sprintf(large,"uid,student_id,student_name\n00112233,S1,");
    memset(large+used,'A',49); used+=49; large[used++]='\n';
    write_file("0:/DanhSach.csv",large,used);
    CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_FORMAT_ERROR && n==0);
    CASE("overlong student name rejected");
    const char nul_csv[]="uid,student_id,student_name\n00112233,S1,A\0B\n";
    write_file("0:/DanhSach.csv",nul_csv,sizeof nul_csv-1);
    CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_FORMAT_ERROR && n==0);
    CASE("embedded NUL rejected");
    const char lf[]="uid,student_id,student_name\n00112233,S1,NAME";
    write_file("0:/DanhSach.csv",lf,sizeof lf-1);
    CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_OK && n==1);
    CASE("LF and last line without newline accepted");
}

static void test_log(void)
{
    Student_t list[50]; uint16_t n; char text[512],before[512];
    reset_disk(); seed(); CHECK(Storage_LoadStudents(list,50,&n)==STORAGE_OK);
    CardEvent_t card={.uid={0,0x11,0x22,0x33},.uid_len=4,.uptime_ms=12345};
    CHECK(Storage_AppendAttendance(&card,&list[0])==STORAGE_OK);
    CHECK(read_file("0:/history.csv",text,sizeof text)>0);
    CHECK(!strcmp(text,"uptime_ms,uid,student_id\r\n12345,00112233,SV001\r\n"));
    strcpy(before,text); card.uptime_ms=23456;
    CHECK(Storage_AppendAttendance(&card,&list[0])==STORAGE_OK);
    read_file("0:/history.csv",text,sizeof text);
    CHECK(!strncmp(text,before,strlen(before)));
    CHECK(!strcmp(text+strlen(before),"23456,00112233,SV001\r\n"));
    CASE("append preserves prior log and creates header only once");
    unsigned previous_writes=write_calls;
    card.uid[0]=1;
    CHECK(Storage_AppendAttendance(&card,&list[0])==STORAGE_INVALID_ARGUMENT);
    CHECK(write_calls==previous_writes); card.uid[0]=0;
    CHECK(Storage_AppendAttendance(NULL,&list[0])==STORAGE_INVALID_ARGUMENT);
    CASE("UID mismatch and NULL do not write log");
    const char partial[]="uptime_ms,uid,student_id\r\n123,00112233,S1";
    write_file("0:/history.csv",partial,sizeof partial-1);
    CHECK(Storage_AppendAttendance(&card,&list[0])==STORAGE_FORMAT_ERROR);
    read_file("0:/history.csv",text,sizeof text); CHECK(!strcmp(text,partial));
    CASE("partial final log line rejected without append");
    write_file("0:/history.csv","wrong\n",6);
    CHECK(Storage_AppendAttendance(&card,&list[0])==STORAGE_FORMAT_ERROR);
    CASE("wrong history header rejected");
    write_file("0:/history.csv","",0);
    CHECK(Storage_AppendAttendance(&card,&list[0])==STORAGE_OK);
    fail_write=1;
    CHECK(Storage_AppendAttendance(&card,&list[0])!=STORAGE_OK);
    fail_write=0;
    CASE("disk write/sync failure never reports success");
}

static void test_manager_and_app(void)
{
    reset_disk(); seed(); App_Init(); CHECK(StorageManager_IsReady());
    uint16_t n; const Student_t *list=StorageManager_GetStudents(&n);
    CHECK(list && n==3);
    CardEvent_t card={.uid={0,0x11,0x22,0x33},.uid_len=4,.uptime_ms=88};
    DisplayEvent_t display;
    CHECK(App_SubmitCard(&card)==ATTENDANCE_ACCEPTED);
    CHECK(App_TakeDisplayEvent(&display) && display.status==APP_CARD_ACCEPTED);
    CHECK(!strcmp(display.student_id,"SV001"));
    CHECK(!App_TakeDisplayEvent(&display));
    CASE("public card input publishes one successful display event");
    CHECK(App_SubmitCard(&card)==ATTENDANCE_BUSY);
    feedback_done();
    unsigned previous_writes=write_calls;
    for (unsigned i=0;i<20;++i) {
        card.uptime_ms+=1000;
        CHECK(App_SubmitCard(&card)==ATTENDANCE_DUPLICATE);
    }
    CHECK(write_calls==previous_writes && !Feedback_IsBusy());
    CHECK(!App_TakeDisplayEvent(&display));
    CASE("held UID never logs again, even as time advances");
    tick+=100; App_NotifyNoCard(tick);
    tick+=100; App_NotifyNoCard(tick);
    CHECK(App_SubmitCard(&card)==ATTENDANCE_DUPLICATE);
    CASE("short false no-card result does not rearm");
    tick+=100; App_NotifyNoCard(tick); tick+=1000; App_NotifyNoCard(tick);
    CHECK(App_SubmitCard(&card)==ATTENDANCE_DUPLICATE);
    CASE("long gap without polling does not prove removal");
    tick+=100; App_NotifyNoCard(tick); App_NotifyReaderError();
    tick+=100; App_NotifyNoCard(tick);
    CHECK(App_SubmitCard(&card)==ATTENDANCE_DUPLICATE);
    CASE("reader error is not treated as removal");
    absent_stable();
    CHECK(App_SubmitCard(&card)==ATTENDANCE_ACCEPTED);
    CHECK(App_TakeDisplayEvent(&display)); feedback_done();
    CASE("stable absence followed by re-presentation permits new log");
    CardEvent_t unknown={.uid={0xFF,0xFF,0xFF,0xFF},.uid_len=4};
    previous_writes=write_calls;
    CHECK(App_SubmitCard(&unknown)==ATTENDANCE_UNKNOWN);
    CHECK(App_TakeDisplayEvent(&display) && display.status==APP_CARD_UNKNOWN);
    CHECK(write_calls==previous_writes); feedback_done();
    CHECK(App_SubmitCard(&unknown)==ATTENDANCE_DUPLICATE);
    CASE("unknown UID publishes error once and never logs");
    absent_stable(); fail_write=1;
    CHECK(App_SubmitCard(&card)==ATTENDANCE_STORAGE_FAILED);
    CHECK(App_TakeDisplayEvent(&display) && display.status==APP_STORAGE_ERROR);
    CHECK(!StorageManager_IsReady()); feedback_done(); fail_write=0;
    tick+=1100; StorageManager_Process(); CHECK(StorageManager_IsReady());
    CHECK(App_SubmitCard(&card)==ATTENDANCE_DUPLICATE);
    CASE("write failure invalidates SD and never automatically retries same card");
    present=0; tick+=1100; StorageManager_Process();
    CHECK(!StorageManager_IsReady());
    n=99; CHECK(!StorageManager_GetStudents(&n) && n==0);
    present=1; CHECK(Storage_Init()==STORAGE_OK);
    const char changed[]="uid,student_id,student_name\nAABBCCDD,NEW01,NEW STUDENT\n";
    write_file("0:/DanhSach.csv",changed,sizeof changed-1);
    tick+=1100; StorageManager_Process();
    list=StorageManager_GetStudents(&n);
    CHECK(list && n==1 && !strcmp(list[0].student_id,"NEW01"));
    CASE("remove/reinsert replaces list with changed CSV");
    present=0; tick+=1100; StorageManager_Process(); present=1;
    CHECK(Storage_Init()==STORAGE_OK);
    write_file("0:/DanhSach.csv","bad\n",4);
    tick+=1100; StorageManager_Process();
    CHECK(!StorageManager_IsReady());
    CHECK(StorageManager_GetStatus()==STORAGE_FORMAT_ERROR);
    n=99; CHECK(!StorageManager_GetStudents(&n) && n==0);
    CASE("invalid replacement CSV never becomes active list");
    seed(); tick+=1100; StorageManager_Process(); CHECK(StorageManager_IsReady());
    CASE("corrected CSV recovers without MCU reset");
    /* Boundary: subtraction of HAL ticks must survive uint32 wrap. */
    Attendance_Init(); CHECK(Attendance_ProcessCard(&card,&display)==ATTENDANCE_ACCEPTED);
    uint32_t time=UINT32_MAX-300U;
    for (unsigned i=0;i<8;++i) { Attendance_NotifyNoCard(time); time+=100; }
    CHECK(Attendance_ProcessCard(&card,&display)==ATTENDANCE_ACCEPTED);
    CASE("absence timing survives HAL tick wrap");
}

static void test_feedback(void)
{
    Feedback_Init(); CHECK(led==GPIO_PIN_SET && buzzer==GPIO_PIN_RESET);
    CHECK(Feedback_Start(APP_CARD_ACCEPTED));
    CHECK(led==GPIO_PIN_RESET && buzzer==GPIO_PIN_SET);
    CHECK(!Feedback_Start(APP_CARD_UNKNOWN));
    tick+=249; Feedback_Process(); CHECK(Feedback_IsBusy());
    tick+=1; Feedback_Process(); CHECK(!Feedback_IsBusy());
    CHECK(led==GPIO_PIN_SET && buzzer==GPIO_PIN_RESET);
    CHECK(Feedback_Start(APP_CARD_UNKNOWN));
    for (unsigned i=0;i<4;++i) { tick+=250; Feedback_Process(); CHECK(Feedback_IsBusy()); }
    tick+=250; Feedback_Process(); CHECK(!Feedback_IsBusy());
    CHECK(led==GPIO_PIN_SET && buzzer==GPIO_PIN_RESET);
    CASE("feedback polarity, one/three pulses and busy rejection");
}

int main(void)
{
    test_csv(); test_log(); test_manager_and_app(); test_feedback();
    printf("ALL %u CASE GROUPS PASSED\n",tests);
    return 0;
}
