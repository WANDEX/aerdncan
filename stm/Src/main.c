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

typedef struct {
  float Rl;           // Resistor Load       (Ohms)
  float Rs;           // Resistor Shunt      (Ohms)
  float Vref;         // Reference Voltage   (Volts)
  int16_t PIN_Is;     // Input  Current pin
  int16_t PIN_Temp;   // Temperature pin
  int16_t PIN_Vout;   // Output Voltage pin
} ina169_c;           // constants

typedef struct {
  struct uavcan_equipment_power_BatteryInfo* const data;
  volatile ina169_c const* c; // constants
  uint8_t  buf[UAVCAN_EQUIPMENT_POWER_BATTERYINFO_MAX_SIZE];
  uint32_t buf_len;
} ina169_t; // self-contained ina169 data structure

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/**
 * @brief total count of batteries UAV has.
 *
 * @see batteries[AERDNCAN_BATTERY_CNT]
 */
#ifndef AERDNCAN_BATTERY_CNT
#define AERDNCAN_BATTERY_CNT 1
#endif//AERDNCAN_BATTERY_CNT

/**
 * @note max battery id is uint8_t -> 256 || 0x100!
 */
#ifndef AERDNCAN_BATTERY_ID_BASE
#define AERDNCAN_BATTERY_ID_BASE 0x10
#endif//AERDNCAN_BATTERY_ID_BASE

/**
 * @note max length is 31 chars!
 */
#ifndef AERDNCAN_BATTERY_MODEL_NAME
#define AERDNCAN_BATTERY_MODEL_NAME "Dummy Smart Battery v1.1 LiPo"
#endif//AERDNCAN_BATTERY_MODEL_NAME

/**
 * @see CAN1.CalculateBaudRate=875000 bit/s == (bps || bpc || Hz)
 * CAN is a serial bus, 1 bit per cycle processed by design => 1 bit/s == 1 Hz.
 */
#ifndef AERDNCAN_CAN1_TARGET_BITRATE
#define AERDNCAN_CAN1_TARGET_BITRATE 875000
#endif//AERDNCAN_CAN1_TARGET_BITRATE

/**
 * To get via ADC the raw digital step value [0..4095] 12-bit ADC.
 */
#ifndef AERDNCAN_ADC_MAX_VAL
#define AERDNCAN_ADC_MAX_VAL 4095 // ADC_RESOLUTION_12B
// #define AERDNCAN_ADC_MAX_VAL 1023 // ADC_RESOLUTION_10B
// #define AERDNCAN_ADC_MAX_VAL  255 // ADC_RESOLUTION_8B
// #define AERDNCAN_ADC_MAX_VAL   63 // ADC_RESOLUTION_6B
#endif//AERDNCAN_ADC_MAX_VAL

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile uint8_t G_Main_Loop_Update_Event = 0;

/**
 * @brief constants (that may be different) between different batteries etc.
 *
 * @note consider NOT removing static + const, but making another/new instance.
 *
 * FIXME: .Rs is wrong? May currently be calculated improperly!
 *   provided parts of the scheme are hard to understand as one complete whole!
 */
static volatile ina169_c const G_ina169_c = {
  .Rl         = 27.4f * 1000, // *=1000 -> 27.4 (kOhms)
  .Rs         = 15.0f / 1000, // /=1000 -> 15.0 (mOhms)
  // .Rs         = 10.015f    //      -> 10.015 (kOhms) || FIXME: or ???
  .Vref       = 3.3f, // (Volts)
  .PIN_Is     = INA169_BAT_CURRENT_Pin,
  .PIN_Temp   = INA169_BAT_TEMP_Pin,
  .PIN_Vout   = INA169_BAT_VOLTAGE_Pin,
};

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
 * @note avoid re-assigning previously initialized unique fields, such as:
 *   .battery_id, .model_instance_id, .model_name, etc.
 *
 * @retval      0               Success
 */
static uint16_t canard_fill_battery_info_pre(ina169_t *battery)
{
  for (uint8_t i = 0; i < AERDNCAN_BATTERY_CNT; ++i) {
    // memset(battery[i].data, 0, sizeof(*battery[i].data));
    /// fill battery identification
    battery[i].data->battery_id        = AERDNCAN_BATTERY_ID_BASE + i;
    battery[i].data->model_instance_id = AERDNCAN_BATTERY_ID_BASE;
    battery[i].data->model_name.len = strlen(AERDNCAN_BATTERY_MODEL_NAME);
    /// copy battery model name
    strncpy((char*)battery[i].data->model_name.data, AERDNCAN_BATTERY_MODEL_NAME,
            sizeof(battery[i].data->model_name.data));
    battery[i].c = &G_ina169_c; // constants
#if 1 // dbg info
    fprintf(stderr,
        "battery[%d] = {\n"
        "  .battery_id = %d,\n"
        "  .model_instance_id = %lu,\n"
        "  .model_name.data[%d] = \"%s\",\n"
        "}\n",
        i,
        battery[i].data->battery_id,
        battery[i].data->model_instance_id,
        battery[i].data->model_name.len, battery[i].data->model_name.data
    );
#endif
  }
  return 0;
}

/**
 * @see RM0090 - 13.10 Temperature sensor
 * @see RM0090 - 13.11 Battery charge monitoring
 *   Main features
 *     Supported temperature range: –40 to 125 C
 *     Precision: +-1.5 C
 *
 * VSENSE is input
 *
 * @retval      0               Success
 */
static uint16_t canard_ADC_temp(ina169_t *battery)
{
  return 0;
}

/**
 * @brief Taking ina169 pin data, fill: temperature, voltage, current.
 *
 * ina169 datasheet orig formulas:
 *   Vout =  Is * Rs * Rl * gm -- (orig formula)
 *
 * Application Information & Operation (formulas):
 *   Is -- load current || Vs -- Voltage supply || Rs -- Resistor shunt
 *   Vs = Vin = Vin+ - Vin- = Is * Rs -- input Voltage
 *   gm   = Io / Vin = 1000_mA / Vin -- transconductance
 *   Io   = gm * Vin -- output current
 *   Vout = Io * Rl  -- output Voltage
 *
 *   RG1 = RG2 = 1000_Ohms -- ina169 internal Resistors before Rs shunt
 *   Is = (Vout * RG1) / (Rs * Rl)
 *
 * @retval      0     Success
 */
static int16_t canard_ina169_data(ina169_t *battery)
{
  /// easy to store & pass data about individual battery unit as args etc.
  battery->data->current      = 0.f, // Is   (Amps)
  battery->data->temperature  = 0.f, //      (Celsius)
  battery->data->voltage      = 0.f, // Vout (Volts)
  /// @note pre-configured in stm/Src/adc.c MX_ADC1_Init && HAL_ADC_MspInit
  HAL_ADC_Start(&hadc1); // start the ADC conversion
  if (HAL_ADC_PollForConversion(&hadc1, 10 /* ms timeout */) == HAL_OK) {
    uint32_t rds = HAL_ADC_GetValue(&hadc1); // raw digital steps for conversion
    battery->data->voltage =
      ((float)rds * battery->c->Vref) / AERDNCAN_ADC_MAX_VAL;
  }
  HAL_ADC_Stop(&hadc1); // stop the ADC conversion to save power
  // TODO: consider 8.2.3 Offsetting the Output Voltage if needed.
  battery->data->current =
    (battery->data->voltage * 1000) / (battery->c->Rs * battery->c->Rl);
  // TODO: how to get/calculate battery temperature?
  //   * Use temperature sensor VBAT_SENS (What its model, P/N?)
  //   * Calculate/report approximate/critical working conditions based on:
  //     * 6.4 Thermal Information & 6.5 Electrical Characteristics.
  return 0;
}

/**
 * @brief Pushes one frame into the TX buffer, if there is space.
 *
 * @retval      0                       Error
 * @retval      battery data length     Success
 */
static uint32_t canard_fill_battery_info(ina169_t *battery)
{
  if (!canard_ina169_data(battery)) {
    return 0; // fail
  }
  battery->data->average_power_10sec       = 0.f; // float
  battery->data->remaining_capacity_wh     = 0.f; // float
  battery->data->full_charge_capacity_wh   = 0.f; // float
  battery->data->hours_to_full_charge      = 0.f; // float
  battery->data->status_flags              = 0;   // uint16_t
  battery->data->state_of_health_pct       = 0;   // uint8_t
  battery->data->state_of_charge_pct       = 0;   // uint8_t
  battery->data->state_of_charge_pct_stdev = 0;   // uint8_t
  return uavcan_equipment_power_BatteryInfo_encode(
      battery->data,
      battery->buf, true
  );
}

/**
 * @brief Pushes one frame into the TX buffer, if there is space.
 *
 * @retval      0               Success
 * @retval      negative        Error
 */
int16_t canard_send_battery_info(ina169_t *battery)
{
  static char const* const fn  = "canard_send_battery_info()";
  static char const* const fnt = "canardSTM32Transmit()";
  int16_t stat = -1; // error
  /// fill battery_data - CANARD_ENABLE_CANFD required to fit in single frame!
  battery->buf_len = canard_fill_battery_info(battery);
  CanardCANFrame* const frame = { };
  frame->id = 0;
  frame->data_len = battery->buf_len;
  frame->iface_id = 0;
  memcpy(frame->data, battery->buf, battery->buf_len);
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

  ina169_t battery[AERDNCAN_BATTERY_CNT];
  stat = canard_fill_battery_info_pre(battery);
  if (!stat) goto init_fail;
  fprintf(stderr, "[ OK ] batteries info pre-fill done\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (G_Main_Loop_Update_Event) {
      for (uint8_t i = 0; i < AERDNCAN_BATTERY_CNT; ++i) {
        canard_send_battery_info(&battery[i]);
      }
      G_Main_Loop_Update_Event = 0;
    }
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


/**
 * @brief update event within specific time gap without additional delay.
 *
 * How to configure CLK etc.
 *   @see https://deepbluembedded.com/stm32-internal-temperature-sensor-reading-example/
 *
 * Instead of using sleep which adds additional delay time between executions.
 *   HAL_Delay(1000); // wait 1000ms = 1sec
 *
 * stm32 Timer Calculator
 *   @see https://deepbluembedded.com/stm32-timer-calculator/
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0); // Toggle Interrupt Rate Indicator Pin
    G_Main_Loop_Update_Event = 1;
}

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
