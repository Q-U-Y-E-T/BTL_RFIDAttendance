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
#include "app_types.h"
#include "rfid.h"
#include "lcd.h"
#include "storage.h"
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

/* Ket qua chung cua bai test */
static uint8_t sd_test_ok = 0;

/* Danh sach sinh vien duoc doc tu SD vao RAM */
static Student_t students[STORAGE_MAX_STUDENTS];
static uint16_t student_count = 0;

/* Xem bang debugger khi can chan doan */
static volatile Storage_Status_t storage_result = STORAGE_NOT_READY;

/*
 * Giai doan:
 * 1: Test doc/ghi SD
 * 2: Tao danh sach mau neu chua co
 * 3: Khoi tao Storage
 * 4: Doc danh sach vao RAM
 * 5: Hoan thanh
 */
static volatile uint32_t storage_stage = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
uint8_t Test_Storage_Run(void);
uint8_t Test_Storage_CreateSampleList(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  /*
   * Buoc 1: Test ghi va doc lai SDTEST.TXT.
   * File test nay bi ghi de moi lan khoi dong.
   */
  storage_stage = 1;
  sd_test_ok = Test_Storage_Run();

  /*
   * Buoc 2: Tao DanhSach.csv mau neu chua ton tai.
   * Khong ghi de danh sach da co.
   */
  if (sd_test_ok)
  {
    storage_stage = 2;
    sd_test_ok = Test_Storage_CreateSampleList();
  }

  /* Buoc 3: Khoi tao giao dien Storage chung */
  if (sd_test_ok)
  {
    storage_stage = 3;
    storage_result = Storage_Init();
    sd_test_ok = (storage_result == STORAGE_OK);
  }

  /* Buoc 4: Doc va kiem tra danh sach sinh vien */
  if (sd_test_ok)
  {
    storage_stage = 4;

    storage_result = Storage_LoadStudents(
        students,
        STORAGE_MAX_STUDENTS,
        &student_count
    );

    sd_test_ok = (storage_result == STORAGE_OK &&
                  student_count > 0U);
  }

  if (sd_test_ok)
  {
    storage_stage = 5;
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

    /*
     * Nhanh: doc duoc danh sach hop le, co sinh vien.
     * Cham: mot buoc test that bai hoac danh sach rong.
     */
    HAL_Delay(sd_test_ok ? 100U : 1000U);
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
