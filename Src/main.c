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
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include <stdlib.h>
#include <stdarg.h>
#include "Mfft.h"
#include "ad9959.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MHZ(x)    ((uint32_t)((x) * 1000000UL))
#define KHZ(x)    ((uint32_t)((x) * 1000UL))
#define HZ(x)     ((uint32_t)(x))

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */

  /* 初始化 FFT 模块 */
  MFFT_Init();
  printf("Init AD9959...\r\n");
  Init_AD9959();
  HAL_Delay(100);
  // AD9959_SetChannel(0, 1000, 0, MHZ(3));//通道0，幅度1000，相位0，频率20MHz
  AD9959_SetChannel(0, 1000, 0, KHZ(10));//通道0，幅度1000，相位0，频率10kHz
  // AD9959_SetChannel(1, 1000, 0, MHZ(5));//通道0，幅度1000，相位0，频率20MHz
  // AD9959_SetChannel(3, 1000, 0, MHZ(1));//通道0，幅度1000，相位0，频率20MHz
  AD9959_IO_Update();
  printf("AD9959 Init Done\r\n");

  printf("System started\r\n");
  printf("Sweep: %d ~ %d Hz, %d pts\r\n",
         MFFT_SWEEP_START_FREQ, MFFT_SWEEP_END_FREQ, MFFT_SWEEP_POINTS);

  /* 启动 ADC2（从设备） */
  if (HAL_ADC_Start(&hadc2) != HAL_OK)
  {
    printf("ERR: ADC2 start failed\r\n");
    Error_Handler();
  }

  /* 以双重同步模式启动 ADC1（主设备）DMA，目标缓冲区在 Mfft.c 中定义 */
  if (HAL_ADCEx_MultiModeStart_DMA(&hadc1,
        (uint32_t *)mfft_dual_buffer, MFFT_SAMPLE_COUNT) != HAL_OK)
  {
    printf("ERR: ADC1 DMA start failed\r\n");
    Error_Handler();
  }

  /* 启动 TIM3 触发 ADC */
  if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
  {
    printf("ERR: TIM3 start failed\r\n");
    Error_Handler();
  }

  printf("Ready\r\n\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t freq_hz;

    // MFFT_PerformSweep();
    // HAL_Delay(3000);
    for (freq_hz = KHZ(1); freq_hz <= KHZ(100); freq_hz += KHZ(1))
    {
      AD9959_SetChannel(0, 1000, 0, freq_hz);
      AD9959_IO_Update();
      printf("DDS CH0: %lu Hz\r\n", (unsigned long)freq_hz);
      HAL_Delay(1000);
    }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/******************************************************************************
 * 功  能： printf函数支持代码
 * 说  明： 加入以下代码, 使用printf函数时, 不再需要选择use MicroLIB
 *          复制本函数到你的工程，即可使用
 * 参  数：
 * 返回值：
 * 备  注： 最后修改_2020年07月15日
 ******************************************************************************/
#pragma import(__use_no_semihosting)

struct __FILE
{
  int handle;
}; /* 标准库需要的支持函数 */

FILE __stdout;
void _sys_exit(int x)
{
  x = x;
}

int fputc(int ch, FILE *f)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0x02);
  return ch;
}

/**
 * @brief  ADC DMA 传输完成回调，设置 Mfft 模块的 buffer_ready 标志
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    mfft_buffer_ready = 1;
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
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
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
