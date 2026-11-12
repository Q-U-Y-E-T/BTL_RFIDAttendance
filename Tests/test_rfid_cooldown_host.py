#!/usr/bin/env python3
"""Test the actual task source with simulated FreeRTOS time and queue calls."""
from pathlib import Path
import os
import subprocess
import tempfile

project = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="rfid-cooldown-test-") as directory:
    temporary = Path(directory)
    (temporary / "FreeRTOS.h").write_text('''#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef uint32_t TickType_t;
#define pdPASS 1
#define pdFAIL 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configASSERT(x) assert(x)
#endif
''')
    (temporary / "queue.h").write_text('''#include "FreeRTOS.h"
typedef void *QueueHandle_t;
BaseType_t xQueueSend(QueueHandle_t,const void*,TickType_t);
''')
    (temporary / "task.h").write_text('''#include "FreeRTOS.h"
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void*);
BaseType_t xTaskCreate(TaskFunction_t,const char*,uint32_t,void*,UBaseType_t,TaskHandle_t*);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t);
void vTaskDelayUntil(TickType_t*,TickType_t);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t);
''')
    test = r'''#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "task_rfid_read.c"

QueueHandle_t g_card_event_queue = (void*)1;
static TickType_t ticks;
static TaskFunction_t entry;
static jmp_buf finished;
static CardEvent_t inputs[64], copies[64];
static TickType_t input_times[64];
static unsigned input_count, input_index, received, fail_sends;

BaseType_t xTaskCreate(TaskFunction_t task,const char* name,uint32_t stack,
                      void* argument,UBaseType_t priority,TaskHandle_t* handle) {
    assert(!strcmp(name,"RFID_Read") && stack==384 && priority==2);
    assert(argument==NULL);
    entry=task;*handle=(void*)2;return pdPASS;
}
RFID_Status_t RFID_Init(void) {return RFID_OK;}
RFID_Status_t RFID_ReadCard(CardEvent_t* card) {
    assert(input_index<input_count);
    ticks=input_times[input_index];
    *card=inputs[input_index];card->uptime_ms=ticks;
    return RFID_OK;
}
TickType_t xTaskGetTickCount(void) {return ticks;}
void vTaskDelay(TickType_t duration) {ticks+=duration;}
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task) {(void)task;return 128;}
void vTaskDelayUntil(TickType_t* wake,TickType_t period) {
    assert(period==50);*wake+=period;ticks=*wake;
    if (++input_index==input_count) longjmp(finished,1);
}
BaseType_t xQueueSend(QueueHandle_t queue,const void* item,TickType_t timeout) {
    assert(queue==(void*)1 && timeout==100);
    if(fail_sends){--fail_sends;ticks+=100;return pdFAIL;}
    assert(received<64);
    memcpy(&copies[received++],item,sizeof(CardEvent_t));return pdPASS;
}

static CardEvent_t make_card(unsigned number) {
    CardEvent_t card={0};
    card.uid_len=4;card.uid[0]=0;card.uid[1]=0x69;
    card.uid[2]=(uint8_t)(number>>8);card.uid[3]=(uint8_t)number;
    return card;
}
static void reset_state(void) {
    memset(cooldown_entries,0,sizeof(cooldown_entries));
    memset((void*)&g_rfid_task_stats,0,sizeof(g_rfid_task_stats));
    rfid_task_handle=NULL;
    input_count=input_index=received=fail_sends=0;
    ticks=0;
}
static int accept(const CardEvent_t* card,TickType_t now) {
    int slot=Cooldown_FindSlot(card,now,120000U);
    if(slot>=0)Cooldown_Record((unsigned)slot,card,now);
    return slot;
}
static void run_task(void) {
    ticks=input_times[0];
    assert(RFID_Task_Start()==pdPASS);
    if(!setjmp(finished))entry(NULL);
}

int main(void) {
    CardEvent_t a=make_card(1),b=make_card(2),long_uid=a;
    reset_state();
    assert(accept(&a,0)>=0);
    assert(accept(&a,1)==COOLDOWN_SUPPRESSED);
    assert(accept(&a,119999)==COOLDOWN_SUPPRESSED);
    assert(accept(&a,120000)>=0);
    assert(accept(&a,120001)==COOLDOWN_SUPPRESSED);
    puts("PASS first scan, 119999 ms blocked, 120000 ms allowed");

    reset_state();
    assert(accept(&a,0)>=0 && accept(&b,5000)>=0);
    assert(accept(&a,120000)>=0);
    assert(accept(&b,120000)==COOLDOWN_SUPPRESSED);
    assert(accept(&b,125000)>=0);
    puts("PASS independent cooldown per UID");

    reset_state();
    assert(accept(&a,0)>=0);
    for(TickType_t now=50;now<120000;now+=50)
        assert(accept(&a,now)==COOLDOWN_SUPPRESSED);
    assert(accept(&a,120000)>=0);
    puts("PASS held-card scans do not extend the deadline");

    reset_state();
    assert(accept(&a,0)>=0);
    long_uid.uid_len=7;long_uid.uid[6]=0xAA;
    assert(accept(&long_uid,0)>=0);
    long_uid.uid_len=10;long_uid.uid[9]=0xBB;
    assert(accept(&long_uid,0)>=0);
    assert(accept(&a,50)==COOLDOWN_SUPPRESSED);
    b=a;b.uid[0]=1;assert(accept(&b,50)>=0);
    for(unsigned len=0;len<=255;len+=51){b.uid_len=len;assert(accept(&b,100)==COOLDOWN_INVALID_UID);}
    puts("PASS UID 4/7/10, leading zero, byte/length comparison and invalid lengths");

    reset_state();
    TickType_t start=UINT32_MAX-60000U;
    assert(accept(&a,start)>=0);
    assert(accept(&a,(TickType_t)(start+119999U))==COOLDOWN_SUPPRESSED);
    assert(accept(&a,(TickType_t)(start+120000U))>=0);
    puts("PASS tick wrap-around");

    reset_state();
    for(unsigned i=0;i<50;i++){b=make_card(i);assert(accept(&b,0)>=0);}
    b=make_card(50);assert(accept(&b,100)==COOLDOWN_FULL);
    b=make_card(0);assert(accept(&b,100)==COOLDOWN_SUPPRESSED);
    b=make_card(50);assert(accept(&b,120000)>=0);
    puts("PASS 50-slot capacity, no eviction of active UIDs, reuse expired entry");

    reset_state();
    b=make_card(2);
    assert(accept(&a,0)>=0 && accept(&b,100000)>=0);
    assert(accept(&b,120000)==COOLDOWN_SUPPRESSED);
    assert(accept(&a,120000)>=0);
    int slot=Cooldown_FindSlot(&a,120001,0);
    assert(slot>=0);
    puts("PASS expired slot cannot bypass a later active UID; zero interval permits scans");

    reset_state();
    input_count=4;fail_sends=2;
    const TickType_t times[]={0,250,120150,120200};
    for(unsigned i=0;i<input_count;i++){inputs[i]=a;input_times[i]=times[i];}
    run_task();
    assert(received==2 && g_rfid_task_stats.cards_queued==2);
    assert(g_rfid_task_stats.queue_waits==2 && g_rfid_task_stats.cards_suppressed==2);
    assert(copies[0].uptime_ms==0 && copies[1].uptime_ms==120200);
    assert(!memcmp(copies[0].uid,a.uid,4));
    puts("PASS real task entry: queue retry retains event; cooldown begins at successful enqueue");

    reset_state();
    input_count=52;
    for(unsigned i=0;i<51;i++){inputs[i]=make_card(i);input_times[i]=50U*i;}
    inputs[51]=make_card(0);input_times[51]=2550;
    run_task();
    assert(received==50 && g_rfid_task_stats.cooldown_full==1);
    assert(g_rfid_task_stats.cards_suppressed==1);
    puts("PASS real task entry: full table is reported without dropping active history");
    puts("ALL COOLDOWN HOST TESTS PASSED (not a board timing test)");
}
'''
    source = temporary / "test.c"
    source.write_text(test)
    executable = temporary / "test"
    subprocess.run([
        "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-g",
        "-I" + str(temporary), "-I" + str(project / "App/Inc"),
        "-I" + str(project / "App/Src"), "-I" + str(project / "Modules/RC522"),
        str(source), "-o", str(executable)
    ], check=True)
    environment = os.environ.copy()
    environment["ASAN_OPTIONS"] = "detect_leaks=0"
    subprocess.run([str(executable)], check=True, env=environment)
