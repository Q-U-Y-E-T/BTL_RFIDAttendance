/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "storage_manager.h"
#include "feedback.h"
#include "attendance.h"

#include <stddef.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/*
 * demo_stage:
 * 0: Cho danh sach san sang de test the hop le.
 * 1: Da test the hop le, cho test the khong dang ky.
 * 2: Da hoan thanh hai luot test.
 */
static uint8_t demo_stage = 0;
static uint32_t demo_tick = 0;

static CardEvent_t demo_card;
static DisplayEvent_t demo_display;

/* Xem ket qua tung test bang debugger. */
static volatile AttendanceResult_t demo_valid_result =
    ATTENDANCE_NOT_READY;

static volatile AttendanceResult_t demo_unknown_result =
    ATTENDANCE_NOT_READY;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static uint8_t MakeUnknownCard(CardEvent_t *card);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * Tao UID 4 byte khong co trong danh sach.
 * Danh sach toi da 50 sinh vien, nen thu 256 gia tri
 * byte dau se tim duoc mot UID chua dang ky.
 */
static uint8_t MakeUnknownCard(CardEvent_t *card)
{
  uint16_t count = 0;

  const Student_t *students =
      StorageManager_GetStudents(&count);

  if (card == NULL || students == NULL)
  {
    return 0;
  }

  memset(card, 0, sizeof *card);
  card->uid_len = 4;

  for (unsigned int value = 0; value < 256U; ++value)
  {
    uint8_t found = 0;

    card->uid[0] = (uint8_t)value;

    for (uint16_t i = 0; i < count; ++i)
    {
      if (students[i].uid_len == card->uid_len &&
          memcmp(students[i].uid,
                 card->uid,
                 card->uid_len) == 0)
      {
        found = 1;
        break;
      }
    }

    if (!found)
    {
      return 1;
    }
  }

  return 0;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  MX_SPI2_Init();

  /* USER CODE BEGIN 2 */

  Feedback_Init();
  StorageManager_Init();

  demo_stage = 0;
  demo_tick = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    Feedback_Process();

    /*
     * Tam hoan thao tac SD trong luc phat mau bao,
     * tranh thoi gian cho SD lam keo dai nhip LED.
     */
    if (!Feedback_IsBusy())
    {
      StorageManager_Process();
    }

    /*
     * Test 1: gia lap the cua sinh vien dau tien.
     * Ghi them mot dong vao history.csv.
     */
    if (demo_stage == 0 &&
        !Feedback_IsBusy() &&
        StorageManager_IsReady() &&
        (uint32_t)(HAL_GetTick() - demo_tick) >= 2000U)
    {
      uint16_t count = 0;

      const Student_t *students =
          StorageManager_GetStudents(&count);

      if (students != NULL && count > 0)
      {
        memset(&demo_card, 0, sizeof demo_card);

        demo_card.uid_len = students[0].uid_len;

        memcpy(
            demo_card.uid,
            students[0].uid,
            demo_card.uid_len
        );

        demo_card.uptime_ms = HAL_GetTick();

        demo_valid_result = Attendance_ProcessCard(
            &demo_card,
            &demo_display
        );

        (void)Feedback_Start(demo_display.status);

        demo_stage = 1;
        demo_tick = HAL_GetTick();
      }
    }

    /*
     * Test 2: gia lap UID khong co trong danh sach.
     * Du kien bao ba nhip, khong ghi them log.
     */
    if (demo_stage == 1 &&
        !Feedback_IsBusy() &&
        StorageManager_IsReady() &&
        (uint32_t)(HAL_GetTick() - demo_tick) >= 3000U)
    {
      if (MakeUnknownCard(&demo_card))
      {
        demo_card.uptime_ms = HAL_GetTick();

        demo_unknown_result = Attendance_ProcessCard(
            &demo_card,
            &demo_display
        );

        (void)Feedback_Start(demo_display.status);

        demo_stage = 2;
      }
    }

    HAL_Delay(5);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK
                             | RCC_CLOCKTYPE_SYSCLK
                             | RCC_CLOCKTYPE_PCLK1
                             | RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  __disable_irq();

  while (1)
  {
  }

  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief Reports the source file and line of an assert error.
  * @param file: source file name
  * @param line: source line number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  (void)file;
  (void)line;

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
