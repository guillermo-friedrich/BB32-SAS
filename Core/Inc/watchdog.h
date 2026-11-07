/*
 * watchdog.h
 *
 *  Created on: Oct 14, 2025
 *      Author: guillermo
 */

#ifndef INC_WATCHDOG_H_
#define INC_WATCHDOG_H_

// Dirección del registro RCC_CSR
#define RCC_CSR          (*(volatile uint32_t *)0x40023874)

// Dirección del registro DBGMCU_APB1_FZ para STM32F427
#define DBGMCU_APB1_FZ   (*(volatile uint32_t *)0xE0042008)

// Tipo de reset
typedef enum {
    RESET_CAUSE_UNKNOWN = 0,
    RESET_CAUSE_LOW_POWER,
    RESET_CAUSE_WINDOW_WATCHDOG,
    RESET_CAUSE_INDEPENDENT_WATCHDOG,
    RESET_CAUSE_SOFTWARE,
    RESET_CAUSE_POWER_ON,
    RESET_CAUSE_EXTERNAL_PIN,
    RESET_CAUSE_BROWNOUT
} ResetCause_t;

void IWDG_Init_Register(void);
void Debug_FreezeWatchdog(void);
ResetCause_t GetResetCause(void);

//#define IWDG_REFRESH()   (*((volatile uint32_t *)0x40003000) = 0xAAAA)

#define IWDG_REFRESH()   IWDG->KR=0xAAAA

#define IWDG_PR_DIV_4   0
#define IWDG_PR_DIV_8   1
#define IWDG_PR_DIV_16  2
#define IWDG_PR_DIV_32  3
#define IWDG_PR_DIV_64  4
#define IWDG_PR_DIV_128 5
#define IWDG_PR_DIV_256 6

#endif /* INC_WATCHDOG_H_ */
