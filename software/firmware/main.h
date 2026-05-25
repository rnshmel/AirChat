// AirChat PCB main firmware
// Author: Richard Shmel

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g0xx_hal.h"

void Error_Handler(void);

void AirChat_UART_IDLE_Callback(void);

#define GPIO0_Pin GPIO_PIN_15
#define GPIO0_GPIO_Port GPIOC
#define CSn_Pin GPIO_PIN_4
#define CSn_GPIO_Port GPIOA
#define RX_LED_Pin GPIO_PIN_5
#define RX_LED_GPIO_Port GPIOA
#define TX_LED_Pin GPIO_PIN_7
#define TX_LED_GPIO_Port GPIOA
#define GPIO1_Pin GPIO_PIN_11
#define GPIO1_GPIO_Port GPIOA

#ifdef __cplusplus
}
#endif

#endif
