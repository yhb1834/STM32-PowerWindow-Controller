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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
	WINDOW_IDLE,
	WINDOW_MANUAL_UP,
	WINDOW_MANUAL_DOWN,
	WINDOW_AUTO_UP,
	WINDOW_AUTO_DOWN,
	WINDOW_ANTI_PINCH_REVERSE,
	WINDOW_FAULT
}WindowState_t;

typedef enum
{
	WINDOW_CMD_NONE = 0,
	WINDOW_CMD_TOGGLE
}WindowCommand_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define INA219_ADDR (0x40 << 1)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* Definitions for InputTask */
osThreadId_t InputTaskHandle;
const osThreadAttr_t InputTask_attributes = {
  .name = "InputTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ControlTask */
osThreadId_t ControlTaskHandle;
const osThreadAttr_t ControlTask_attributes = {
  .name = "ControlTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for DiagTask */
osThreadId_t DiagTaskHandle;
const osThreadAttr_t DiagTask_attributes = {
  .name = "DiagTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for windowCommandQueue */
osMessageQueueId_t windowCommandQueueHandle;
const osMessageQueueAttr_t windowCommandQueue_attributes = {
  .name = "windowCommandQueue"
};
/* USER CODE BEGIN PV */
WindowState_t windowState = WINDOW_IDLE;

uint32_t prevButtonState = 0;
uint32_t lastButtonTick = 0;
uint32_t lastLedTick = 0;

uint32_t motorStartTick = 0;
uint8_t motorStarting = 0;

uint8_t nextDirectionUp = 1;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C1_Init(void);
void StartInputTask(void *argument);
void StartControlTask(void *argument);
void StartDiagTask(void *argument);

/* USER CODE BEGIN PFP */
void Window_SetState(WindowState_t newState);
void Window_ButtonProcess(void);
void Window_ControlTask(void);

void Motor_SetDuty(uint8_t duty);
void Motor_Stop(void);
void Motor_RunUp(void);
void Motor_RunDown(void);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void Window_SetState(WindowState_t newState)
{
	windowState = newState;

	switch(windowState)
	{
	case WINDOW_IDLE:
	{
		char msg[] = "[WINDOW] IDLE\r\n";

		HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg)-1, HAL_MAX_DELAY);

		break;
	}

	case WINDOW_MANUAL_UP:
	{
		char msg[] = "[WINDOW] MANUAL_UP\r\n";

		HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg)-1, HAL_MAX_DELAY);

		break;
	}

	case WINDOW_MANUAL_DOWN:
	{
		char msg[] = "[WINDOW] MANUAL_DOWN\r\n";

		HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg)-1, HAL_MAX_DELAY);

		break;
	}
	default:
		break;
	}

}

void Window_ButtonProcess(void)
{
	uint32_t now = HAL_GetTick();
	uint32_t buttonState = BSP_PB_GetState(BUTTON_USER);

	if ((buttonState == 1) && (prevButtonState == 0) && (now - lastButtonTick >= 50))
	{
		lastButtonTick = now;

		WindowCommand_t cmd = WINDOW_CMD_TOGGLE;

		osMessageQueuePut(windowCommandQueueHandle, &cmd, 0, 0);
	}

	prevButtonState = buttonState;
}

void Window_ControlTask(void)
{
	switch (windowState)
	{
	case WINDOW_IDLE:
		Motor_Stop();
		break;

	case WINDOW_MANUAL_UP:
		Motor_RunUp();

		if(motorStarting && (HAL_GetTick() - motorStartTick >= 200))
		{
			Motor_SetDuty(30);
			motorStarting = 0;
		}
		break;

	case WINDOW_MANUAL_DOWN:
		Motor_RunDown();

		if (motorStarting && (HAL_GetTick() - motorStartTick >= 200))
		{
			Motor_SetDuty(30);
			motorStarting = 0;
		}

		break;

	case WINDOW_AUTO_UP:
		Motor_RunUp();
		break;

	case WINDOW_AUTO_DOWN:
		Motor_RunDown();
		break;

	case WINDOW_ANTI_PINCH_REVERSE:
		Motor_RunDown();
		break;

	case WINDOW_FAULT:
		Motor_Stop();
		break;

	default:
		Motor_Stop();
		Window_SetState(WINDOW_FAULT);
		break;
	}


}

void Motor_SetDuty(uint8_t duty)
{
	if (duty > 100)
	{
		duty = 100;
	}

	uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1;
	uint32_t pulse = (period * duty) / 100;

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
}

void Motor_RunUp(void)
{
    HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port,
                      MOTOR_IN1_Pin,
                      GPIO_PIN_SET);

    HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port,
                      MOTOR_IN2_Pin,
                      GPIO_PIN_RESET);

    if (motorStarting == 0)
    {
    	Motor_SetDuty(50);
    	motorStartTick = HAL_GetTick();
    	motorStarting = 1;
    }
}

void Motor_RunDown(void)
{
    HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port,
                      MOTOR_IN1_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port,
                      MOTOR_IN2_Pin,
                      GPIO_PIN_SET);

    if (motorStarting == 0)
    {
    	 Motor_SetDuty(50);

    	 motorStartTick = HAL_GetTick();
    	 motorStarting = 1;

    }

}

void Motor_Stop(void)
{
    Motor_SetDuty(0);

    HAL_GPIO_WritePin(MOTOR_IN1_GPIO_Port,
                      MOTOR_IN1_Pin,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(MOTOR_IN2_GPIO_Port,
                      MOTOR_IN2_Pin,
                      GPIO_PIN_RESET);

    motorStarting = 0;
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
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  char msg[] = "PowerWindow ECU Started\r\n";

  HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg) - 1, HAL_MAX_DELAY);

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  if (HAL_I2C_IsDeviceReady(&hi2c1,
                            INA219_ADDR,
                            3,
                            100) == HAL_OK)
  {
      char msg[] = "[INA219] Device Ready\r\n";

      HAL_UART_Transmit(&huart2,
                        (uint8_t *)msg,
                        sizeof(msg) - 1,
                        HAL_MAX_DELAY);
  }
  else
  {
      char msg[] = "[INA219] Device NOT Ready\r\n";

      HAL_UART_Transmit(&huart2,
                        (uint8_t *)msg,
                        sizeof(msg) - 1,
                        HAL_MAX_DELAY);
  }

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of windowCommandQueue */
  windowCommandQueueHandle = osMessageQueueNew (8, sizeof(uint32_t), &windowCommandQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of InputTask */
  InputTaskHandle = osThreadNew(StartInputTask, NULL, &InputTask_attributes);

  /* creation of ControlTask */
  ControlTaskHandle = osThreadNew(StartControlTask, NULL, &ControlTask_attributes);

  /* creation of DiagTask */
  DiagTaskHandle = osThreadNew(StartDiagTask, NULL, &DiagTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Initialize leds */
  BSP_LED_Init(LED2);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  uint32_t now = HAL_GetTick();
	  /* LED periodic task : 500ms */
	  if (now - lastLedTick >= 500)
	  {
		  lastLedTick = now;
		  BSP_LED_Toggle(LED2);
	  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 4199;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, MOTOR_IN1_Pin|MOTOR_IN2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : MOTOR_IN1_Pin MOTOR_IN2_Pin */
  GPIO_InitStruct.Pin = MOTOR_IN1_Pin|MOTOR_IN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartInputTask */
/**
  * @brief  Function implementing the InputTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartInputTask */
void StartInputTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
	  Window_ButtonProcess();
	  osDelay(10);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartControlTask */
/**
* @brief Function implementing the ControlTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartControlTask */
void StartControlTask(void *argument)
{
  /* USER CODE BEGIN StartControlTask */

    WindowCommand_t cmd;

    for (;;)
    {
        if (osMessageQueueGet(windowCommandQueueHandle,
                              &cmd,
                              NULL,
                              0) == osOK)
        {
            if (cmd == WINDOW_CMD_TOGGLE)
            {
                if (windowState == WINDOW_IDLE)
                {
                    /* 정지 상태라면 다음 방향으로 출발 */
                    if (nextDirectionUp)
                    {
                        Window_SetState(WINDOW_MANUAL_UP);
                    }
                    else
                    {
                        Window_SetState(WINDOW_MANUAL_DOWN);
                    }
                }
                else
                {
                    /* 움직이는 중이면 정지 */
                    Window_SetState(WINDOW_IDLE);

                    /* 다음 출발 방향 변경 */
                    nextDirectionUp = !nextDirectionUp;
                }
            }
        }

        Window_ControlTask();

        osDelay(10);
    }

  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartDiagTask */
/**
* @brief Function implementing the DiagTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDiagTask */
void StartDiagTask(void *argument)
{
  /* USER CODE BEGIN StartDiagTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(100);
  }
  /* USER CODE END StartDiagTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
