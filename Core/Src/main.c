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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  RESET_CTRL_I2C_RECOVER_NONE = 0U,
  RESET_CTRL_I2C_RECOVER_LISTEN_ENABLE_FAIL = 1U,
  RESET_CTRL_I2C_RECOVER_LISTEN_BAD_STATE = 2U,
  RESET_CTRL_I2C_RECOVER_ADDR_RX_SETUP_FAIL = 3U,
  RESET_CTRL_I2C_RECOVER_ADDR_TX_SETUP_FAIL = 4U,
  RESET_CTRL_I2C_RECOVER_RX_DATA_SETUP_FAIL = 5U,
  RESET_CTRL_I2C_RECOVER_ERROR_CALLBACK = 6U
} ResetController_I2CRecoverReason;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SLG_BASE_I2C_ADDRESS_7BIT 0x10U
#define SLG_BLOCK1_I2C_ADDRESS_7BIT 0x11U
#define SLG_VIRTUAL_OUT_REG_ADDR 0x01DBU
#define SLG_VIRTUAL_OUT_WORD_ADDR 0xDBU
#define SLG_VIRTUAL_OUT_MASK 0x7FU
#define SLG_VIRTUAL_OUT_DEFAULT 0x7FU
#define RESET_BUTTON_DEBOUNCE_MS 25U
#define RESET_OUTPUT_RELEASE_DELAY_MS 25U

#define SLG_VOUT0_BIT (1U << 0)
#define SLG_VOUT1_BIT (1U << 1)
#define SLG_VOUT2_BIT (1U << 2)
#define SLG_VOUT3_BIT (1U << 3)
#define SLG_VOUT6_BIT (1U << 6)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
static volatile uint16_t reset_ctrl_reg_index = SLG_VIRTUAL_OUT_REG_ADDR;
static volatile uint16_t reset_ctrl_i2c_block_base = 0U;
static volatile uint8_t reset_ctrl_rx_is_word_address = 1U;
static volatile uint8_t reset_ctrl_rx_byte = 0U;
static volatile uint8_t reset_ctrl_tx_byte = 0U;
static volatile uint8_t reset_ctrl_virtual_out = SLG_VIRTUAL_OUT_DEFAULT;
static volatile uint8_t reset_ctrl_i2c_recover_requested = 0U;
static volatile uint32_t reset_ctrl_i2c_recover_count = 0U;
static volatile uint32_t reset_ctrl_i2c_last_error = 0U;
static volatile uint32_t reset_ctrl_i2c_last_isr = 0U;
static volatile uint32_t reset_ctrl_i2c_last_state = 0U;
static volatile uint8_t reset_ctrl_i2c_last_recover_reason = RESET_CTRL_I2C_RECOVER_NONE;
static volatile uint8_t reset_ctrl_i2c_last_hal_status = HAL_OK;
static volatile uint8_t reset_button_sample = 0U;
static volatile uint8_t reset_button_debounced = 0U;
static volatile uint32_t reset_button_last_change_tick = 0U;
static volatile uint32_t reset_outputs_release_tick = 0U;
static volatile uint8_t reset_outputs_release_pending = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
static void ResetController_InitDebounce(void);
static void ResetController_Service(void);
static void ResetController_StartI2CListen(void);
static void ResetController_ServiceI2C(void);
static void ResetController_RequestI2CRecover(
    I2C_HandleTypeDef *hi2c, ResetController_I2CRecoverReason reason,
    HAL_StatusTypeDef hal_status);
static uint8_t ResetController_ReadRegister(uint16_t reg);
static void ResetController_WriteRegister(uint16_t reg, uint8_t value);
static uint16_t ResetController_NormalizeRegisterAddress(uint16_t reg);
static uint16_t ResetController_BlockBaseFromAddress(uint16_t addr_match_code);
static void ResetController_ApplyOutputs(void);
static uint8_t ResetController_ReadPinHigh(GPIO_TypeDef *gpio_port,
                                           uint16_t gpio_pin);
static void ResetController_SetOpenDrainReset(GPIO_TypeDef *gpio_port,
                                              uint16_t gpio_pin,
                                              uint8_t release_reset);
static void ResetController_HandleResetButtonInterrupt(void);
static uint8_t ResetController_ResetButtonReleased(void);
static uint8_t ResetController_PsPorReleased(void);
static uint8_t ResetController_PsOutputsMayRelease(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  ResetController_InitDebounce();
  ResetController_ApplyOutputs();
  ResetController_StartI2CListen();

  HAL_GPIO_WritePin(SOM_PS_POR_B_GPIO_Port, SOM_PS_POR_B_Pin, GPIO_PIN_RESET);
  HAL_Delay(RESET_OUTPUT_RELEASE_DELAY_MS);
  ResetController_ApplyOutputs();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    ResetController_Service();
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00300617;
  hi2c1.Init.OwnAddress1 = 0x22;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SOM_PS_POR_B_GPIO_Port, SOM_PS_POR_B_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB,
                    PS_RESET_OUT1_B_Pin | PS_RESET_OUT2_B_Pin |
                        PS_RESET_OUT3_B_Pin | PS_RESET_OUT4_B_Pin |
                        PS_RESET_OUT7_B_Pin,
                    GPIO_PIN_RESET);

  /*Configure GPIO pins : PS_RESET_IN1_B_Pin PS_RESET_IN2_B_Pin
     PL_RESET_IN1_B_Pin PL_RESET_IN2_B_Pin PL_RESET_IN3_B_Pin */
  GPIO_InitStruct.Pin = PS_RESET_IN1_B_Pin | PS_RESET_IN2_B_Pin |
                        PL_RESET_IN1_B_Pin | PL_RESET_IN2_B_Pin |
                        PL_RESET_IN3_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : SOM_PS_POR_B_Pin */
  GPIO_InitStruct.Pin = SOM_PS_POR_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SOM_PS_POR_B_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RESET_BUTTON_B_Pin */
  GPIO_InitStruct.Pin = RESET_BUTTON_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(RESET_BUTTON_B_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CC_PL_PGOOD_Pin */
  GPIO_InitStruct.Pin = CC_PL_PGOOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(CC_PL_PGOOD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CC_PS_PGOOD_Pin */
  GPIO_InitStruct.Pin = CC_PS_PGOOD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CC_PS_PGOOD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PS_RESET_OUT1_B_Pin PS_RESET_OUT2_B_Pin
     PS_RESET_OUT3_B_Pin PS_RESET_OUT4_B_Pin PS_RESET_OUT7_B_Pin */
  GPIO_InitStruct.Pin = PS_RESET_OUT1_B_Pin | PS_RESET_OUT2_B_Pin |
                        PS_RESET_OUT3_B_Pin | PS_RESET_OUT4_B_Pin |
                        PS_RESET_OUT7_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void ResetController_InitDebounce(void) {
  reset_button_sample =
      ResetController_ReadPinHigh(RESET_BUTTON_B_GPIO_Port, RESET_BUTTON_B_Pin);
  reset_button_debounced = reset_button_sample;
  reset_button_last_change_tick = HAL_GetTick();
  reset_outputs_release_tick = HAL_GetTick() + RESET_OUTPUT_RELEASE_DELAY_MS;
  reset_outputs_release_pending = 1U;
}

static void ResetController_Service(void) {
  uint8_t sample =
      ResetController_ReadPinHigh(RESET_BUTTON_B_GPIO_Port, RESET_BUTTON_B_Pin);
  uint32_t now = HAL_GetTick();

  if (sample != reset_button_sample) {
    reset_button_sample = sample;
    reset_button_last_change_tick = now;
  }

  if ((sample != reset_button_debounced) &&
      ((now - reset_button_last_change_tick) >= RESET_BUTTON_DEBOUNCE_MS)) {
    reset_button_debounced = sample;
    ResetController_ApplyOutputs();
  }

  ResetController_ApplyOutputs();
  ResetController_ServiceI2C();
}

static void ResetController_HandleResetButtonInterrupt(void) {
  reset_button_sample =
      ResetController_ReadPinHigh(RESET_BUTTON_B_GPIO_Port, RESET_BUTTON_B_Pin);
  reset_button_last_change_tick = HAL_GetTick();
}

static void ResetController_StartI2CListen(void) {
  if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_READY) {
    reset_ctrl_rx_is_word_address = 1U;
    if (HAL_I2C_EnableListen_IT(&hi2c1) != HAL_OK) {
      ResetController_RequestI2CRecover(
          &hi2c1, RESET_CTRL_I2C_RECOVER_LISTEN_ENABLE_FAIL, HAL_ERROR);
    }
  } else if (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_LISTEN) {
    ResetController_RequestI2CRecover(
        &hi2c1, RESET_CTRL_I2C_RECOVER_LISTEN_BAD_STATE, HAL_BUSY);
  }
}

static void ResetController_RequestI2CRecover(
    I2C_HandleTypeDef *hi2c, ResetController_I2CRecoverReason reason,
    HAL_StatusTypeDef hal_status) {
  reset_ctrl_i2c_last_recover_reason = (uint8_t)reason;
  reset_ctrl_i2c_last_hal_status = (uint8_t)hal_status;

  if ((hi2c != NULL) && (hi2c->Instance == I2C1)) {
    reset_ctrl_i2c_last_error = HAL_I2C_GetError(hi2c);
    reset_ctrl_i2c_last_isr = hi2c->Instance->ISR;
    reset_ctrl_i2c_last_state = HAL_I2C_GetState(hi2c);
  }

  reset_ctrl_i2c_recover_requested = 1U;
}

static void ResetController_ServiceI2C(void) {
  if (reset_ctrl_i2c_recover_requested == 0U) {
    return;
  }

  reset_ctrl_i2c_recover_requested = 0U;
  reset_ctrl_i2c_recover_count++;

  reset_ctrl_rx_is_word_address = 1U;
  reset_ctrl_i2c_block_base = 0U;
  reset_ctrl_rx_byte = 0U;
  reset_ctrl_tx_byte = 0U;

  (void)HAL_I2C_DeInit(&hi2c1);
  MX_I2C1_Init();
  ResetController_StartI2CListen();
}

static uint8_t ResetController_ReadRegister(uint16_t reg) {
  uint8_t value = 0U;

  switch (ResetController_NormalizeRegisterAddress(reg)) {
  case SLG_VIRTUAL_OUT_REG_ADDR:
    value = reset_ctrl_virtual_out & SLG_VIRTUAL_OUT_MASK;
    break;

  default:
    value = 0xFFU;
    break;
  }

  return value;
}

static void ResetController_WriteRegister(uint16_t reg, uint8_t value) {
  switch (ResetController_NormalizeRegisterAddress(reg)) {
  case SLG_VIRTUAL_OUT_REG_ADDR:
    reset_ctrl_virtual_out = value & SLG_VIRTUAL_OUT_MASK;
    ResetController_ApplyOutputs();
    break;

  default:
    break;
  }
}

static uint16_t ResetController_NormalizeRegisterAddress(uint16_t reg) {
  if ((reg == SLG_VIRTUAL_OUT_REG_ADDR) || (reg == SLG_VIRTUAL_OUT_WORD_ADDR)) {
    return SLG_VIRTUAL_OUT_REG_ADDR;
  }

  return reg;
}

static uint16_t ResetController_BlockBaseFromAddress(uint16_t addr_match_code) {
  if ((addr_match_code == SLG_BLOCK1_I2C_ADDRESS_7BIT) ||
      (addr_match_code == (SLG_BLOCK1_I2C_ADDRESS_7BIT << 1))) {
    return 0x0100U;
  }

  return 0U;
}

static void ResetController_ApplyOutputs(void) {
  uint8_t ps_por_release =
      ResetController_ResetButtonReleased() &&
      ResetController_ReadPinHigh(CC_PS_PGOOD_GPIO_Port, CC_PS_PGOOD_Pin);

  HAL_GPIO_WritePin(SOM_PS_POR_B_GPIO_Port, SOM_PS_POR_B_Pin,
                    (ps_por_release != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  uint8_t ps_outputs_release = ResetController_PsOutputsMayRelease();

  ResetController_SetOpenDrainReset(
      PS_RESET_OUT1_B_GPIO_Port, PS_RESET_OUT1_B_Pin,
      ResetController_ReadPinHigh(PS_RESET_IN1_B_GPIO_Port,
                                  PS_RESET_IN1_B_Pin) &&
          ps_outputs_release &&
          ((reset_ctrl_virtual_out & SLG_VOUT0_BIT) != 0U));
  ResetController_SetOpenDrainReset(
      PS_RESET_OUT2_B_GPIO_Port, PS_RESET_OUT2_B_Pin,
      ResetController_ReadPinHigh(PS_RESET_IN2_B_GPIO_Port,
                                  PS_RESET_IN2_B_Pin) &&
          ps_outputs_release &&
          ((reset_ctrl_virtual_out & SLG_VOUT1_BIT) != 0U));
  ResetController_SetOpenDrainReset(
      PS_RESET_OUT3_B_GPIO_Port, PS_RESET_OUT3_B_Pin,
      ps_outputs_release && ((reset_ctrl_virtual_out & SLG_VOUT2_BIT) != 0U));
  ResetController_SetOpenDrainReset(
      PS_RESET_OUT4_B_GPIO_Port, PS_RESET_OUT4_B_Pin,
      ps_outputs_release && ((reset_ctrl_virtual_out & SLG_VOUT3_BIT) != 0U));
  ResetController_SetOpenDrainReset(
      PS_RESET_OUT7_B_GPIO_Port, PS_RESET_OUT7_B_Pin,
      ps_outputs_release && ((reset_ctrl_virtual_out & SLG_VOUT6_BIT) != 0U));
}

static uint8_t ResetController_ReadPinHigh(GPIO_TypeDef *gpio_port,
                                           uint16_t gpio_pin) {
  return (HAL_GPIO_ReadPin(gpio_port, gpio_pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static void ResetController_SetOpenDrainReset(GPIO_TypeDef *gpio_port,
                                              uint16_t gpio_pin,
                                              uint8_t release_reset) {
  HAL_GPIO_WritePin(gpio_port, gpio_pin,
                    (release_reset != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t ResetController_ResetButtonReleased(void) {
  return (reset_button_debounced != 0U) ? 1U : 0U;
}

static uint8_t ResetController_PsPorReleased(void) {
  return ResetController_ReadPinHigh(SOM_PS_POR_B_GPIO_Port, SOM_PS_POR_B_Pin);
}

static uint8_t ResetController_PsOutputsMayRelease(void) {
  uint8_t stable_inputs =
      ResetController_ReadPinHigh(CC_PS_PGOOD_GPIO_Port, CC_PS_PGOOD_Pin) &&
      ResetController_PsPorReleased() && ResetController_ResetButtonReleased();
  uint32_t now = HAL_GetTick();

  if (stable_inputs == 0U) {
    reset_outputs_release_pending = 1U;
    reset_outputs_release_tick = now + RESET_OUTPUT_RELEASE_DELAY_MS;
    return 0U;
  }

  if (reset_outputs_release_pending != 0U) {
    if ((int32_t)(now - reset_outputs_release_tick) < 0) {
      return 0U;
    }

    reset_outputs_release_pending = 0U;
  }

  return 1U;
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection,
                          uint16_t AddrMatchCode) {
  HAL_StatusTypeDef status = HAL_OK;

  if (hi2c->Instance != I2C1) {
    return;
  }

  reset_ctrl_i2c_block_base =
      ResetController_BlockBaseFromAddress(AddrMatchCode);

  if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
    reset_ctrl_rx_is_word_address = 1U;
    status = HAL_I2C_Slave_Seq_Receive_IT(
        hi2c, (uint8_t *)&reset_ctrl_rx_byte, 1U, I2C_FIRST_FRAME);
    if (status != HAL_OK) {
      ResetController_RequestI2CRecover(
          hi2c, RESET_CTRL_I2C_RECOVER_ADDR_RX_SETUP_FAIL, status);
    }
  } else {
    reset_ctrl_tx_byte = ResetController_ReadRegister(reset_ctrl_reg_index);
    status = HAL_I2C_Slave_Seq_Transmit_IT(
        hi2c, (uint8_t *)&reset_ctrl_tx_byte, 1U, I2C_FIRST_AND_LAST_FRAME);
    if (status != HAL_OK) {
      ResetController_RequestI2CRecover(
          hi2c, RESET_CTRL_I2C_RECOVER_ADDR_TX_SETUP_FAIL, status);
    }
  }
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  HAL_StatusTypeDef status = HAL_OK;

  if (hi2c->Instance != I2C1) {
    return;
  }

  if (reset_ctrl_rx_is_word_address != 0U) {
    reset_ctrl_reg_index = reset_ctrl_i2c_block_base | reset_ctrl_rx_byte;
    reset_ctrl_rx_is_word_address = 0U;
    status = HAL_I2C_Slave_Seq_Receive_IT(
        hi2c, (uint8_t *)&reset_ctrl_rx_byte, 1U, I2C_NEXT_FRAME);
    if (status != HAL_OK) {
      ResetController_RequestI2CRecover(
          hi2c, RESET_CTRL_I2C_RECOVER_RX_DATA_SETUP_FAIL, status);
    }
  } else {
    ResetController_WriteRegister(reset_ctrl_reg_index, reset_ctrl_rx_byte);
    reset_ctrl_reg_index++;
  }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c->Instance != I2C1) {
    return;
  }

  reset_ctrl_reg_index++;
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c->Instance != I2C1) {
    return;
  }

  reset_ctrl_rx_is_word_address = 1U;
  ResetController_StartI2CListen();
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  uint32_t error_code = 0U;

  if (hi2c->Instance != I2C1) {
    return;
  }

  reset_ctrl_rx_is_word_address = 1U;
  error_code = HAL_I2C_GetError(hi2c);
  reset_ctrl_i2c_last_error = error_code;
  reset_ctrl_i2c_last_isr = hi2c->Instance->ISR;

  if ((error_code & ~HAL_I2C_ERROR_AF) == HAL_I2C_ERROR_NONE) {
    ResetController_StartI2CListen();
  } else {
    ResetController_RequestI2CRecover(
        hi2c, RESET_CTRL_I2C_RECOVER_ERROR_CALLBACK, HAL_ERROR);
  }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == RESET_BUTTON_B_Pin) {
    ResetController_HandleResetButtonInterrupt();
  }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == RESET_BUTTON_B_Pin) {
    ResetController_HandleResetButtonInterrupt();
  }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
