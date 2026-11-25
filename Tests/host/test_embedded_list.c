#define TEST_REAL_EMBEDDED_LIST 1
#define main unused_storage_test_main
#include "test_suite.c"
#undef main
#include "embedded_list_data.h"

static void expect_embedded(void)
{
    char buffer[8192];
    size_t size=read_file("0:/DanhSach.csv",buffer,sizeof buffer);
    CHECK(size==sizeof embedded_csv && !memcmp(buffer,embedded_csv,size));
    uint16_t n=0;const Student_t *list=StorageManager_GetStudents(&n);
    CHECK(list && n==EMBEDDED_STUDENT_COUNT);
    CHECK(list[0].uid_len==4 && list[0].uid[0]==0);
}
int main(void)
{
    reset_disk();App_Init();
    CHECK(!StorageManager_IsReady());CHECK(write_calls==0);
    CASE("boot init does not access SD on the small main/interrupt stack");
    App_Process();CHECK(StorageManager_IsReady());expect_embedded();
    CHECK(g_list_update_result==LIST_UPDATE_CREATED);
    CASE("first Storage task pass creates embedded CSV and loads it automatically");
    unsigned previous=write_calls;
    App_Init();App_Process();expect_embedded();
    CHECK(g_list_update_result==LIST_UPDATE_UNCHANGED && write_calls==previous);
    CASE("same firmware reset checks CSV but does not rewrite or create another backup");
    const char old_csv[]="uid,student_id,student_name\nDEADBEEF,OLD01,OLD STUDENT\n";
    write_file("0:/DanhSach.csv",old_csv,sizeof old_csv-1);
    const char history[]="uptime_ms,uid,student_id\r\n1,DEADBEEF,OLD01\r\n";
    write_file("0:/history.csv",history,sizeof history-1);
    App_Init();App_Process();expect_embedded();
    CHECK(g_list_update_result==LIST_UPDATE_UPDATED);
    char buffer[8192];
    read_file("0:/DSBAK00.CSV",buffer,sizeof buffer);CHECK(!strcmp(buffer,old_csv));
    read_file("0:/history.csv",buffer,sizeof buffer);CHECK(!strcmp(buffer,history));
    CASE("different SD list is backed up, new embedded list activated, history untouched");
    present=0;tick+=1100;App_Process();CHECK(!StorageManager_IsReady());
    present=1;tick+=1100;App_Process();expect_embedded();
    CASE("card reinsert automatically runs sync and reload without reflashing");
    present=0;tick+=1100;App_Process();present=1;
    CHECK(Storage_Init()==STORAGE_OK);
    write_file("0:/DanhSach.csv",old_csv,sizeof old_csv-1);
    fail_write=1;tick+=1100;App_Process();
    CHECK(!StorageManager_IsReady() && StorageManager_GetStatus()==STORAGE_IO_ERROR);
    fail_write=0;tick+=1100;App_Process();expect_embedded();
    CHECK(StorageManager_IsReady());
    CASE("failed auto update never publishes partial students and recovers when SD works again");
    CardEvent_t card={.uid={0,0x11,0x22,0x33},.uid_len=4,.uptime_ms=tick};
    CHECK(App_SubmitCard(&card)==ATTENDANCE_ACCEPTED);
    DisplayEvent_t display;CHECK(App_TakeDisplayEvent(&display));
    CHECK(display.status==APP_CARD_ACCEPTED && !strcmp(display.student_id,"S1"));
    read_file("0:/history.csv",buffer,sizeof buffer);
    CHECK(strstr(buffer,",00112233,S1\r\n")!=NULL);
    CASE("after auto update the normal attendance path logs and publishes student identity");
    previous=write_calls;
    CHECK(App_SubmitCard(&card)==ATTENDANCE_DUPLICATE && write_calls==previous);
    CASE("held-card suppression remains effective after auto provisioning");
    printf("ALL %u EMBEDDED LIST CASE GROUPS PASSED\n",tests);
    return 0;
}
