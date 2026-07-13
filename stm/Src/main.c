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
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>              // fprintf
#include <string.h>

#include <canard_stm32.h>

#include "uavcan.equipment.power.BatteryInfo.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/**
 * @note max length is 31 chars!
 */
#ifndef AERDNCAN_BATTERY_MODEL_NAME
#define AERDNCAN_BATTERY_MODEL_NAME "Dummy Battery Name"
#endif//AERDNCAN_BATTERY_MODEL_NAME

/**
 * @see CAN1.CalculateBaudRate=875000 bit/s == (bps || bpc || Hz)
 * CAN is a serial bus, 1 bit per cycle processed by design => 1 bit/s == 1 Hz.
 */
#ifndef AERDNCAN_CAN1_TARGET_BITRATE
#define AERDNCAN_CAN1_TARGET_BITRATE 875000
#endif//AERDNCAN_CAN1_TARGET_BITRATE

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

/**
 * @brief Missing implementation of usleep for the bare-metal target.
 *
 * Fixes linking error / missing implementation of usleep().
 * canard_stm32.c:139:(.text.waitMSRINAKBitStateChange+0x34):
 * undefined reference to usleep.
 */
int usleep(int usec) {
    if (usec == 0) return 0;
    int ms = usec / 1000; // convert microseconds to milliseconds
    if (ms == 0) ms = 1;  // minimum delay of 1ms
    HAL_Delay(ms);
    return 0;
}

/**
 * @brief Initializes the CAN controller at the specified bit rate.
 *
 * @retval      0               Success
 * @retval      negative        Error
 */
int16_t canard_init(void)
{
  static char const* const fn  = "canard_init()";
  static char const* const fni = "canardSTM32Init()";
  static char const* const fnt = "canardSTM32ComputeCANTimings()";
  int16_t stat = -1; // error
  /// @see RM0090 - 32.7.7 Bit timing
  CanardSTM32CANTimings timings = {
    .bit_rate_prescaler = hcan1.Init.Prescaler, // [1, 1024]
    .bit_segment_1      = hcan1.Init.TimeSeg1,  // [1, 16]
    .bit_segment_2      = hcan1.Init.TimeSeg2,  // [1, 8]
    .max_resynchronization_jump_width = 1       // [1, 4] (1 is recommended)
  };
#if 1 // try to compute timings automatically
  uint32_t const peripheral_clock_rate = HAL_RCC_GetPCLK1Freq();
  stat = canardSTM32ComputeCANTimings(peripheral_clock_rate,
         AERDNCAN_CAN1_TARGET_BITRATE, &timings);
  if (stat < 0) { // error
    fprintf(stderr, "%s ERROR: %s -> %d\n", fn, fnt, stat);
    return stat;
  }
#endif
  /// automatic TX abort on error should be used - dynamic node ID allocation.
  stat = canardSTM32Init(&timings, CanardSTM32IfaceModeAutomaticTxAbortOnError);
  if (stat < 0) { // error
    /// The clock of the CAN module must be enabled
    /// If CAN2 is used, CAN1 must be also enabled!
    fprintf(stderr, "%s ERROR: %s -> %d\n", fn, fni, stat);
    return stat;
  }
  return 0;
}

/**
 * @brief Pushes one frame into the TX buffer, if there is space.
 *
 * @retval battery data length.
 */
static uint32_t canard_fill_battery_info(uint8_t* buffer)
{
  // TODO fill with: temperature, voltage, current - from the ina169 pin data.
  struct uavcan_equipment_power_BatteryInfo pkt = {
    .temperature               = 0.f, // float
    .voltage                   = 0.f, // float
    .current                   = 0.f, // float
    .average_power_10sec       = 0.f, // float
    .remaining_capacity_wh     = 0.f, // float
    .full_charge_capacity_wh   = 0.f, // float
    .hours_to_full_charge      = 0.f, // float
    .status_flags              = 0,   // uint16_t
    .state_of_health_pct       = 0,   // uint8_t
    .state_of_charge_pct       = 0,   // uint8_t
    .state_of_charge_pct_stdev = 0,   // uint8_t
    .battery_id                = 0,   // uint8_t
    .model_instance_id         = 0,   // uint32_t
  };
  pkt.model_name.len = strlen(AERDNCAN_BATTERY_MODEL_NAME);
  strncpy((char*)pkt.model_name.data, AERDNCAN_BATTERY_MODEL_NAME,
          sizeof(pkt.model_name.data));
  return uavcan_equipment_power_BatteryInfo_encode(&pkt, buffer, true);
}

/**
 * @brief Pushes one frame into the TX buffer, if there is space.
 *
 * @retval      0               Success
 * @retval      negative        Error
 */
int16_t canard_send_battery_info(void)
{
  static char const* const fn  = "canard_send_battery_info()";
  static char const* const fnt = "canardSTM32Transmit()";
  int16_t stat = -1; // error
  /// fill battery_data - CANARD_ENABLE_CANFD required to fit in single frame!
  uint8_t  battery_data[UAVCAN_EQUIPMENT_POWER_BATTERYINFO_MAX_SIZE];
  uint32_t battery_data_len = canard_fill_battery_info(battery_data);
  CanardCANFrame* const frame = { };
  frame->id = 0;
  frame->data_len = battery_data_len;
  frame->iface_id = 0;
  memcpy(frame->data, battery_data, battery_data_len);
  /// CAN send battery info, making sure that it fits in into a single frame!
  stat = canardSTM32Transmit(frame);
  if (stat < 0) { // error
    fprintf(stderr, "%s ERROR: %s -> %d\n", fn, fnt, stat);
    return stat;
  }
  if (stat != 1) { // error
    fprintf(stderr, "%s ERROR: %s No space in the buffer!\n", fn, fnt);
    return stat * -1;
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
  MX_USART3_UART_Init();
  MX_CAN1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  int16_t stat = -1; // error
  stat = canard_init();
  if (!stat) goto init_fail;
  fprintf(stderr, "[ OK ] init done\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    canard_send_battery_info();
    HAL_Delay(1000); // wait 1000ms = 1sec
  }
  return 0;
init_fail:
  fprintf(stderr, "[FAIL] init fail -> %d\n", stat);
  /// additional cleanup if needed.
  return stat;
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
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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

/* USER CODE END 4 */

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
