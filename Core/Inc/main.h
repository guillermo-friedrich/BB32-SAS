/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CARD_RST_Pin GPIO_PIN_8
#define CARD_RST_GPIO_Port GPIOD
#define CARD_DI_Pin GPIO_PIN_9
#define CARD_DI_GPIO_Port GPIOD
#define LED_GREEN_Pin GPIO_PIN_12
#define LED_GREEN_GPIO_Port GPIOD
#define LED_ORANGE_Pin GPIO_PIN_13
#define LED_ORANGE_GPIO_Port GPIOD
#define LED_RED_Pin GPIO_PIN_14
#define LED_RED_GPIO_Port GPIOD
#define LED_BL_Pin GPIO_PIN_15
#define LED_BL_GPIO_Port GPIOD
#define DS2401_PIN_Pin GPIO_PIN_6
#define DS2401_PIN_GPIO_Port GPIOC
#define BuzzCTRL_Pin GPIO_PIN_3
#define BuzzCTRL_GPIO_Port GPIOD
#define CARD_IN_Pin GPIO_PIN_4
#define CARD_IN_GPIO_Port GPIOD
#define CARD_DO_Pin GPIO_PIN_6
#define CARD_DO_GPIO_Port GPIOD
#define CARD_CLK_Pin GPIO_PIN_7
#define CARD_CLK_GPIO_Port GPIOD
#define ETH_RESET_Pin GPIO_PIN_9
#define ETH_RESET_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
