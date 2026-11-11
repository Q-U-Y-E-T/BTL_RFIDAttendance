/* Selected instead of Core/Src/main.c by make RFID_RTOS_TEST=1. */
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "i2c.h"
#include "app_queues.h"
#include "task_rfid_read.h"
#include <stdio.h>

#ifndef RFID_QUEUE_TEST_PAUSE_MS
#define RFID_QUEUE_TEST_PAUSE_MS 0
#endif

volatile const char *g_rtos_fault_file;
volatile int g_rtos_fault_line;

static void Test_UART_Init(void)
{
    GPIO_InitTypeDef pin = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();
    pin.Pin = GPIO_PIN_9;
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &pin);
    USART1->BRR = (HAL_RCC_GetPCLK2Freq() + 57600U) / 115200U;
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Only the consumer writes UART after the scheduler starts. */
static void Test_UART_Write(const char *text)
{
    uint32_t start = DWT->CYCCNT;
    while (*text != '\0') {
        while ((USART1->SR & USART_SR_TXE) == 0U) {
            if ((uint32_t)(DWT->CYCCNT - start) >= SystemCoreClock / 10U) {
                return;
            }
        }
        USART1->DR = (uint8_t)*text++;
    }
}

void app_assert_failed(const char *file, int line)
{
    g_rtos_fault_file = file;
    g_rtos_fault_line = line;
    __disable_irq();
    Test_UART_Write("\r\nFAULT: ");
    Test_UART_Write(file);
    Test_UART_Write("\r\n");
    for (;;) {
        __NOP();
    }
}

void vApplicationMallocFailedHook(void)
{
    app_assert_failed("FreeRTOS heap exhausted", 0);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    app_assert_failed(name, 0);
}

static void Task_Queue_TestReceiver(void *argument)
{
    static const char hex[] = "0123456789ABCDEF";
    CardEvent_t card;
    uint32_t received = 0U;
    TickType_t last_heartbeat = xTaskGetTickCount();
    char line[192];
    (void)argument;

#if RFID_QUEUE_TEST_PAUSE_MS > 0
    Test_UART_Write("QUEUE TEST: receiver paused; hold a card to fill the queue.\r\n");
    vTaskDelay(pdMS_TO_TICKS(RFID_QUEUE_TEST_PAUSE_MS));
    Test_UART_Write("QUEUE TEST: receiver resumed.\r\n");
#endif

    for (;;) {
        if (xQueueReceive(g_card_event_queue, &card, pdMS_TO_TICKS(100U)) == pdPASS) {
            char uid[21];
            configASSERT(card.uid_len > 0U && card.uid_len <= sizeof(card.uid));
            for (unsigned i = 0; i < card.uid_len; ++i) {
                uid[2U * i] = hex[card.uid[i] >> 4];
                uid[2U * i + 1U] = hex[card.uid[i] & 15U];
            }
            uid[2U * card.uid_len] = '\0';
            ++received;
            (void)snprintf(line, sizeof(line),
                "QUEUE_RX #%lu UID=%s len=%u uptime_ms=%lu sent=%lu waits=%lu\r\n",
                (unsigned long)received, uid, (unsigned)card.uid_len,
                (unsigned long)card.uptime_ms,
                (unsigned long)g_rfid_task_stats.cards_queued,
                (unsigned long)g_rfid_task_stats.queue_waits);
            Test_UART_Write(line);
        }
        if ((TickType_t)(xTaskGetTickCount() - last_heartbeat) >= pdMS_TO_TICKS(1000U)) {
            (void)snprintf(line, sizeof(line),
                "ALIVE init=%u init_errors=%lu read_errors=%lu sent=%lu rx=%lu waits=%lu heap=%lu rfid_stack=%lu rx_stack=%lu\r\n",
                (unsigned)g_rfid_task_stats.init_status,
                (unsigned long)g_rfid_task_stats.init_errors,
                (unsigned long)g_rfid_task_stats.read_errors,
                (unsigned long)g_rfid_task_stats.cards_queued,
                (unsigned long)received,
                (unsigned long)g_rfid_task_stats.queue_waits,
                (unsigned long)xPortGetFreeHeapSize(),
                (unsigned long)g_rfid_task_stats.stack_free_words,
                (unsigned long)uxTaskGetStackHighWaterMark(NULL));
            Test_UART_Write(line);
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            last_heartbeat = xTaskGetTickCount();
        }
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clock = {0};
    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    oscillator.HSIState = RCC_HSI_ON;
    oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    oscillator.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        Error_Handler();
    }
    clock.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                      | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clock.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV1;
    clock.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
        __NOP();
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_I2C1_Init();
    MX_SPI2_Init();
    Test_UART_Init();
    Test_UART_Write("\r\nRFID_TASK_QUEUE_TEST V1 115200-8N1\r\n");
    configASSERT(App_Queues_Init() == pdPASS);
    configASSERT(RFID_Task_Start() == pdPASS);
    configASSERT(xTaskCreate(Task_Queue_TestReceiver, "Queue_Test", 512U,
                            NULL, 1U, NULL) == pdPASS);
    Test_UART_Write("Queue and tasks created; starting scheduler.\r\n");
    vTaskStartScheduler();
    app_assert_failed("scheduler failed to start", 0);
    return 0;
}
