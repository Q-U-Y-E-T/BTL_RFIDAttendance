#define main unused_storage_test_main
#include "test_suite.c"
#undef main
#include "student_list_update.h"

static unsigned reject_commit;
static unsigned short_write;
static FRESULT rename_spy(const TCHAR *old_path, const TCHAR *new_path)
{
    if (reject_commit && !strcmp(old_path,"0:/DSNEW.CSV")) {
        reject_commit=0;
        return FR_DISK_ERR;
    }
    return f_rename(old_path,new_path);
}
static FRESULT write_spy(FIL *file,const void *data,UINT size,UINT *written)
{
    if(short_write && size>0) { short_write=0; return f_write(file,data,size-1U,written); }
    return f_write(file,data,size,written);
}
#define f_rename rename_spy
#define f_write write_spy
#include "../../Modules/Storage/student_list_update.c"
#undef f_write
#undef f_rename

static const char replacement[]="uid,student_id,student_name\r\nAABBCCDD,NEW01,NEW STUDENT\r\n";
static Student_t test_scratch[STORAGE_MAX_STUDENTS];
#define APPLY(data,count,replace) ListUpdate_Apply((const uint8_t *)(data),sizeof(data)-1,count,replace,test_scratch,STORAGE_MAX_STUDENTS)
static void expect_file(const char *path,const char *expected)
{
    char buffer[2048];read_file(path,buffer,sizeof buffer);CHECK(!strcmp(buffer,expected));
}
int main(void)
{
    reset_disk();
    CHECK(APPLY(good_csv,3,0)==LIST_UPDATE_CREATED);
    expect_file("0:/DanhSach.csv",good_csv);
    uint16_t count=0;Student_t list[50];
    CHECK(Storage_LoadStudents(list,50,&count)==STORAGE_OK && count==3);
    CASE("creates complete list, readback matches, real Storage parser accepts it");
    unsigned previous=write_calls;
    CHECK(APPLY(good_csv,3,0)==LIST_UPDATE_UNCHANGED && write_calls==previous);
    CASE("reset with unchanged list is idempotent and writes nothing");
    CHECK(APPLY(replacement,1,0)==LIST_UPDATE_EXISTS && write_calls==previous);
    expect_file("0:/DanhSach.csv",good_csv);
    CASE("default refuses different existing list without changing it");
    write_file("0:/history.csv","KEEP HISTORY\n",13);
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_UPDATED);
    expect_file("0:/DanhSach.csv",replacement);expect_file("0:/DSBAK00.CSV",good_csv);
    expect_file("0:/history.csv","KEEP HISTORY\n");
    CHECK(APPLY(good_csv,3,1)==LIST_UPDATE_UPDATED);
    expect_file("0:/DSBAK00.CSV",good_csv);expect_file("0:/DSBAK01.CSV",replacement);
    CASE("explicit replacement preserves old lists and never touches history");
    CHECK(APPLY(replacement,2,1)==LIST_UPDATE_INVALID);
    expect_file("0:/DanhSach.csv",good_csv);
    CASE("count mismatch fails staged validation before replacing old list");
    const char invalid[]="uid,student_id,student_name\nBADUID,S1,NAME\n";
    CHECK(APPLY(invalid,1,1)==LIST_UPDATE_INVALID);
    expect_file("0:/DanhSach.csv",good_csv);
    CASE("malformed payload cannot replace old list, using actual production parser");
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_UPDATED);
    expect_file("0:/TMPBAK00.CSV",replacement);
    expect_file("0:/TMPBAK01.CSV",invalid);
    CASE("different leftover staging files are archived, never silently deleted");
    reset_disk();
    write_file("0:/DSNEW.CSV",good_csv,sizeof good_csv-1);
    CHECK(APPLY(good_csv,3,0)==LIST_UPDATE_CREATED);
    expect_file("0:/DanhSach.csv",good_csv);
    CASE("valid identical staging file resumes creation after an interrupted run");
    seed();short_write=1;
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_IO_ERROR);
    expect_file("0:/DanhSach.csv",good_csv);
    CASE("short write is an error and leaves active old list intact");
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_UPDATED);
    expect_file("0:/DanhSach.csv",replacement);
    CASE("subsequent reset/retry can recover from a partial staging write");
    reset_disk();seed();reject_commit=1;
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_IO_ERROR);
    expect_file("0:/DanhSach.csv",good_csv);
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_UPDATED);
    CASE("failed final rename attempts rollback; valid stage supports retry");
    reset_disk();seed();fail_write=1;
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_IO_ERROR);
    fail_write=0;CHECK(Storage_Init()==STORAGE_OK);
    expect_file("0:/DanhSach.csv",good_csv);
    CASE("disk write/sync failure does not replace original list");
    reset_disk();seed();char path[32];
    for(unsigned i=0;i<100;++i) {
        (void)snprintf(path,sizeof path,"0:/DSBAK%02u.CSV",i);write_file(path,"KEEP\n",5);
    }
    CHECK(APPLY(replacement,1,1)==LIST_UPDATE_NO_BACKUP_SLOT);
    expect_file("0:/DanhSach.csv",good_csv);expect_file("0:/DSBAK99.CSV","KEEP\n");
    CASE("full backup namespace refuses update instead of overwriting backups");
    CHECK(ListUpdate_Apply(NULL,1,1,0,test_scratch,50)==LIST_UPDATE_INVALID);
    CHECK(APPLY(good_csv,0,0)==LIST_UPDATE_INVALID);
    CHECK(APPLY(good_csv,51,0)==LIST_UPDATE_INVALID);
    count=99;CHECK(Storage_LoadStudentsFile(NULL,list,50,&count)==STORAGE_INVALID_ARGUMENT && count==0);
    CASE("invalid arguments rejected and invalid path clears count");
    printf("ALL %u LIST UPDATE CASE GROUPS PASSED\n",tests);
    return 0;
}
