#define main storage_suite_unused_main
#include "../host/test_suite.c"
#undef main
#include "../../App/Src/app_rtos.c"
#include "../../App/Src/task_rfid_read.c"
#include <setjmp.h>

struct FakeQueue {
    unsigned length, size, head, count;
    unsigned char data[8][128];
};
static struct FakeQueue queues[3];
static unsigned created_queues, loops, limit;
static jmp_buf stop;
static unsigned pulse_starts, pulse_ends;
static GPIO_PinState previous_buzzer;
static unsigned lcd_calls, init_calls, lcd_failures;
static DisplayEvent_t lcd_last;
static unsigned full_release, retired_count;
static ReaderEvent_t retired[16];
static unsigned reader_index, reader_init_failures, reader_init_calls, always_reader_error;
static unsigned task_count;
static unsigned fixed_reader, queue_stall_ms;
static CardEvent_t fixed_card;
static unsigned task_priority[3], task_stack[3];

QueueHandle_t xQueueCreate(UBaseType_t length,UBaseType_t size)
{
    CHECK(created_queues<3 && length<=8 && size<=128);
    struct FakeQueue *q=&queues[created_queues++];
    q->length=length; q->size=size; return q;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t wait)
{
    (void)wait;
    if (!q->count) return pdFAIL;
    memcpy(item,q->data[q->head],q->size);
    q->head=(q->head+1)%q->length; --q->count; return pdPASS;
}
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t wait)
{
    (void)wait;
    if (q->count==q->length) {
        if (full_release && q==g_reader_event_queue) {
            tick += queue_stall_ms;
            CHECK(retired_count<16);
            CHECK(xQueueReceive(q,&retired[retired_count++],0)==pdPASS);
        }
        return pdFAIL;
    }
    memcpy(q->data[(q->head+q->count)%q->length],item,q->size);
    ++q->count; return pdPASS;
}
BaseType_t xQueuePeek(QueueHandle_t q,void *item,TickType_t wait)
{
    (void)wait; if(!q->count) return pdFAIL;
    memcpy(item,q->data[q->head],q->size); return pdPASS;
}
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *item)
{
    CHECK(q->length==1); q->head=0; q->count=1;
    memcpy(q->data[0],item,q->size); return pdPASS;
}
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,
                      void *argument,unsigned priority,TaskHandle_t *handle)
{
    (void)fn;(void)name;(void)argument;
    CHECK(task_count<3);
    task_priority[task_count]=priority;task_stack[task_count]=stack;
    ++task_count; if(handle)*handle=(void *)(uintptr_t)task_count;return pdPASS;
}
void vTaskStartScheduler(void) { longjmp(stop,1); }
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_RUNNING; }
TickType_t xTaskGetTickCount(void) { return tick; }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task) { (void)task;return 123; }
size_t xPortGetFreeHeapSize(void) { return 1000; }
size_t xPortGetMinimumEverFreeHeapSize(void) { return 800; }
void vTaskDelay(TickType_t dt)
{
    if (buzzer!=previous_buzzer) {
        if(buzzer==GPIO_PIN_SET)++pulse_starts;else ++pulse_ends;
        previous_buzzer=buzzer;
    }
    tick+=dt;
    if (++loops>=limit) longjmp(stop,1);
}
void vTaskDelayUntil(TickType_t *last,TickType_t dt)
{ *last+=dt;vTaskDelay(dt); }
LCD_Status_t LCD_Init(void)
{ ++init_calls; return lcd_failures?LCD_ERROR:LCD_OK; }
LCD_Status_t LCD_ShowEvent(const DisplayEvent_t *event)
{ ++lcd_calls;lcd_last=*event;return lcd_failures?LCD_ERROR:LCD_OK; }
RFID_Status_t RFID_Init(void)
{ ++reader_init_calls; if(reader_init_failures){--reader_init_failures;return RFID_ERROR;}return RFID_OK; }
RFID_Status_t RFID_ReadCard(CardEvent_t *card)
{
    if (fixed_reader) { *card=fixed_card; card->uptime_ms=tick; return RFID_OK; }
    RFID_Status_t status=always_reader_error?RFID_ERROR:(reader_index%3==0)?RFID_OK:
                         (reader_index%3==1)?RFID_NO_CARD:RFID_ERROR;
    *card=(CardEvent_t){.uid={0,0x11,0x22,(uint8_t)reader_index},.uid_len=4,.uptime_ms=tick};
    ++reader_index;return status;
}
static void run(void (*task)(void *),unsigned count)
{
    loops=0;limit=count;
    if(!setjmp(stop)) task(NULL);
}
static void reset_runtime(void)
{
    reset_disk();seed();App_Init(); App_Process();
    memset(queues,0,sizeof queues);created_queues=0;
    g_reader_event_queue=g_display_event_queue=g_system_display_queue=NULL;
    CHECK(App_Queues_Init()==pdPASS);
    memset((void *)&g_app_rtos_stats,0,sizeof g_app_rtos_stats);
    rfid_task_handle=NULL;
    memset(cooldown_entries,0,sizeof cooldown_entries);
    memset((void *)&g_rfid_task_stats,0,sizeof g_rfid_task_stats);
    fixed_reader=queue_stall_ms=0;
    previous_buzzer=GPIO_PIN_RESET;pulse_starts=pulse_ends=0;
    lcd_calls=init_calls=lcd_failures=0;full_release=retired_count=0;
    reader_index=reader_init_failures=task_count=reader_init_calls=always_reader_error=0;
}
static void enqueue(RFID_Status_t status,uint32_t when)
{
    ReaderEvent_t e={.status=status,.observed_ms=when,
        .card={.uid={0,0x11,0x22,0x33},.uid_len=4,.uptime_ms=when}};
    CHECK(xQueueSend(g_reader_event_queue,&e,0)==pdPASS);
}
static void scan_fixed(uint32_t when)
{
    tick=when;
    run(Task_RFID_Read,1);
    run(Task_Storage_App,1);
}
static void cooldown_tests(void)
{
    ReaderEvent_t event;
    DisplayEvent_t display;
    reset_runtime();
    fixed_reader=1;
    fixed_card=(CardEvent_t){.uid={0,0x11,0x22,0x33},.uid_len=4};
    scan_fixed(1000);
    CHECK(g_app_rtos_stats.accepted==1);
    CHECK(xQueueReceive(g_display_event_queue,&display,0)==pdPASS);
    scan_fixed(120999);
    CHECK(g_app_rtos_stats.accepted==1 && g_display_event_queue->count==0);
    scan_fixed(121000);
    CHECK(g_app_rtos_stats.accepted==2 && g_rfid_task_stats.cards_queued==2);
    CHECK(xQueueReceive(g_display_event_queue,&display,0)==pdPASS);
    CASE("real producer through Attendance accepts continuously held card exactly at 120000 ms");
    char history[512];
    read_file("0:/history.csv",history,sizeof history);
    unsigned lines=0;for(char *c=history;*c;++c)if(*c=='\n')++lines;
    CHECK(lines==3);
    CHECK(g_rfid_task_stats.cards_suppressed==1);
    CASE("suppressed scan creates neither extra CSV row nor LCD/feedback result");
    for(unsigned i=0;i<8;++i)enqueue(RFID_NO_CARD,122000+i*100);
    run(Task_Storage_App,8);
    scan_fixed(130000);
    CHECK(g_app_rtos_stats.accepted==2 && g_display_event_queue->count==0);
    CASE("removing and replacing a card does not bypass producer's per UID cooldown");
    fixed_card.uid[3]=0x44;
    scan_fixed(130050);
    CHECK(g_rfid_task_stats.cards_queued==3);
    CHECK(xQueueReceive(g_display_event_queue,&display,0)==pdPASS);
    fixed_card.uid[3]=0x33;
    scan_fixed(130100);
    CHECK(g_rfid_task_stats.cards_queued==3 && g_display_event_queue->count==0);
    CASE("another UID remains independent and returning to first UID cannot bypass cooldown");

    reset_runtime();fixed_reader=1;
    fixed_card=(CardEvent_t){.uid={0,0x11,0x22,0x33},.uid_len=4};
    for(unsigned i=0;i<8;++i)enqueue(RFID_NO_CARD,0);
    full_release=1;queue_stall_ms=5000;tick=1000;
    run(Task_RFID_Read,1);
    CHECK(cooldown_entries[0].last_sent==6000);
    ReaderEvent_t queued;
    for(unsigned i=0;i<8;++i) {
        CHECK(xQueueReceive(g_reader_event_queue,&queued,0)==pdPASS);
        if(i==7)CHECK(queued.status==RFID_OK && queued.observed_ms==1000);
    }
    tick=126000;run(Task_RFID_Read,1);
    CHECK(xQueueReceive(g_reader_event_queue,&event,0)==pdPASS && !event.suppressed);
    CHECK(AppInput_Process(&queued)==ATTENDANCE_ACCEPTED);
    CHECK(App_TakeDisplayEvent(&display));
    CHECK(AppInput_Process(&event)==ATTENDANCE_ACCEPTED);
    CASE("queue stall anchors producer timer at enqueue; delayed consumer still accepts next boundary");

    reset_runtime();
    CardEvent_t a={.uid={0,1,2,3},.uid_len=4};
    for(unsigned i=0;i<50;++i) {
        a.uid[3]=(uint8_t)i;
        int slot=Cooldown_FindSlot(&a,100,120000);CHECK(slot>=0);
        Cooldown_Record((unsigned)slot,&a,100);
    }
    a.uid[3]=99;
    CHECK(Cooldown_FindSlot(&a,120099,120000)==COOLDOWN_FULL);
    CHECK(Cooldown_FindSlot(&a,120100,120000)>=0);
    a.uid[3]=49;
    CHECK(Cooldown_FindSlot(&a,120099,120000)==COOLDOWN_SUPPRESSED);
    CASE("50 active UIDs are retained; full table refuses new UID and expired slots are reusable");
    memset(cooldown_entries,0,sizeof cooldown_entries);
    for(unsigned len=4;len<=10;len+=3) {
        a.uid_len=(uint8_t)len;
        int slot=Cooldown_FindSlot(&a,UINT32_MAX-1000U,120000);CHECK(slot>=0);
        Cooldown_Record((unsigned)slot,&a,UINT32_MAX-1000U);
        CHECK(Cooldown_FindSlot(&a,118998,120000)==COOLDOWN_SUPPRESSED);
        CHECK(Cooldown_FindSlot(&a,118999,120000)>=0);
    }
    a.uid_len=5;
    CHECK(Cooldown_FindSlot(&a,0,120000)==COOLDOWN_INVALID_UID);
    CASE("UID 4/7/10, leading zero, length validation and 32 bit timer rollover");
    reset_runtime();
    CardEvent_t known={.uid={0,0x11,0x22,0x33},.uid_len=4,.uptime_ms=UINT32_MAX-1000U};
    CHECK(App_SubmitCard(&known)==ATTENDANCE_ACCEPTED);
    CHECK(App_TakeDisplayEvent(&display));
    known.uptime_ms=118998;
    CHECK(App_SubmitCard(&known)==ATTENDANCE_DUPLICATE);
    known.uptime_ms=118999;
    CHECK(App_SubmitCard(&known)==ATTENDANCE_ACCEPTED);
    CASE("Attendance timeout also handles 32 bit HAL tick rollover");
}
int main(void)
{
    reset_runtime();
    for(unsigned i=0;i<8;++i)enqueue(RFID_OK,100U*i);
    run(Task_Storage_App,8);
    CHECK(g_app_rtos_stats.accepted==1 && g_app_rtos_stats.duplicates==7);
    CHECK(g_display_event_queue->count==1 && !Feedback_IsBusy());
    CHECK(buzzer==GPIO_PIN_RESET);
    DisplayEvent_t display;
    CHECK(xQueueReceive(g_display_event_queue,&display,0)==pdPASS);
    CHECK(display.status==APP_CARD_ACCEPTED && !strcmp(display.student_id,"SV001"));
    CASE("RTOS Storage owner routes held UID to exactly one log/display, never GPIO");
    CHECK(xQueueSend(g_display_event_queue,&display,0)==pdPASS);
    run(Task_UI_Feedback,500);
    CHECK(pulse_starts==1 && pulse_ends==1 && !Feedback_IsBusy());
    CHECK(lcd_calls>=2 && lcd_last.status==APP_READY);
    CASE("UI alone drives one pulse, displays result and returns to ready");

    for(unsigned i=0;i<8;++i)enqueue(RFID_NO_CARD,1000U+100U*i);
    tick=100000;run(Task_Storage_App,8);
    enqueue(RFID_OK,1800);run(Task_Storage_App,1);
    CHECK(g_app_rtos_stats.accepted==2);
    CASE("queued stable absence re-arms using observation time despite delayed consumption");
    reset_runtime();enqueue(RFID_OK,0);run(Task_Storage_App,1);
    CHECK(xQueueReceive(g_display_event_queue,&display,0)==pdPASS);
    enqueue(RFID_NO_CARD,100);enqueue(RFID_NO_CARD,200);
    enqueue(RFID_ERROR,300);enqueue(RFID_NO_CARD,400);
    enqueue(RFID_NO_CARD,500);enqueue(RFID_OK,600);
    run(Task_Storage_App,6);
    CHECK(g_app_rtos_stats.accepted==1 && g_app_rtos_stats.duplicates==1);
    CASE("ordered reader errors break absence chain and cannot cause duplicate logging");
    reset_runtime();enqueue(RFID_OK,0);run(Task_Storage_App,1);
    CHECK(xQueueReceive(g_display_event_queue,&display,0)==pdPASS);
    enqueue(RFID_NO_CARD,100);enqueue(RFID_NO_CARD,2000);enqueue(RFID_OK,2050);
    run(Task_Storage_App,3);CHECK(g_app_rtos_stats.duplicates==1);
    CASE("queue backpressure gaps cannot be mistaken for continuous absence");
    reset_runtime();enqueue(RFID_ERROR,100);run(Task_Storage_App,1);
    CHECK(xQueuePeek(g_system_display_queue,&display,0)==pdPASS);
    CHECK(display.status==APP_READER_ERROR);
    enqueue(RFID_NO_CARD,200);run(Task_Storage_App,1);
    CHECK(xQueuePeek(g_system_display_queue,&display,0)==pdPASS && display.status==APP_READY);
    CASE("system mailbox shows reader failure and recovers on healthy observation");
    reset_runtime();display=(DisplayEvent_t){.status=APP_READY};
    CHECK(xQueueOverwrite(g_system_display_queue,&display)==pdPASS);
    display=(DisplayEvent_t){.status=APP_CARD_UNKNOWN};
    CHECK(xQueueSend(g_display_event_queue,&display,0)==pdPASS);
    lcd_failures=1;run(Task_UI_Feedback,350);
    CHECK(pulse_starts==3 && pulse_ends==3 && g_app_rtos_stats.lcd_errors>0);
    CHECK(g_display_event_queue->count==0);
    lcd_failures=0;run(Task_UI_Feedback,500);CHECK(init_calls>1 && lcd_calls>0);
    CASE("missing LCD does not suppress feedback or block results; retry recovers");
    reset_runtime();full_release=1;run(Task_RFID_Read,10);
    CHECK(g_rfid_task_stats.queue_waits==2 && retired_count==2);
    ReaderEvent_t event;
    for(unsigned i=0;i<10;++i) {
        if(i<retired_count)event=retired[i];
        else CHECK(xQueueReceive(g_reader_event_queue,&event,0)==pdPASS);
        CHECK(event.status==(i%3==0?RFID_OK:i%3==1?RFID_NO_CARD:RFID_ERROR));
        if(event.status==RFID_OK)CHECK(event.card.uid[3]==i);
    }
    CASE("production RFID task queues copied card/absence/error events FIFO across full queue");
    reset_runtime();reader_init_failures=1;run(Task_RFID_Read,1);
    CHECK(xQueueReceive(g_reader_event_queue,&event,0)==pdPASS && event.status==RFID_ERROR);
    CASE("RFID initialization failure reaches consumer instead of pretending absence");
    reset_runtime();full_release=1;always_reader_error=1;
    run(Task_RFID_Read,12);
    CHECK(reader_init_calls==2);
    CASE("repeated read failures reinitialize RC522 without resetting the MCU");
    reset_runtime();
    if(!setjmp(stop))AppRTOS_Start();
    CHECK(task_count==3 && task_priority[0]==3 && task_priority[1]==1 && task_priority[2]==2);
    CHECK(task_stack[0]==384 && task_stack[1]==768 && task_stack[2]==384);
    CASE("startup creates three task owners with intended stacks/priorities");
    reset_runtime();run(Task_UI_Feedback,20);CHECK(lcd_calls==0);
    CASE("UI waits for a real system status rather than showing ready during initial SD sync");
    CHECK(AppInput_Process(NULL)==ATTENDANCE_INVALID_ARGUMENT);
    event.status=(RFID_Status_t)99;
    CHECK(AppInput_Process(&event)==ATTENDANCE_INVALID_ARGUMENT);
    CASE("malformed transport events rejected without a log");
    cooldown_tests();
    printf("ALL %u RTOS ROUTING CASE GROUPS PASSED\n",tests);
    return 0;
}
