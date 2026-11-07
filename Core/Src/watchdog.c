/*
 * watchdog.c
 *
 *  Created on: Oct 14, 2025
 *      Author: guillermo
 */

#include "cashless.h"

/**
 * @brief Inicializa el watchdog timer para un timeout de aprox. 16 s
  *       Con un timeout menor (DIV 64) no permite realizar la reprogramación por la red.
 */
void IWDG_Init_Register(void)
{
    // Habilitar acceso a registros IWDG
    IWDG->KR = 0x5555;  // Enable register access

    // Configurar prescaler
    IWDG->PR = IWDG_PR_DIV_128;  // Aprox. 16 segundos

    // Configurar valor de recarga
    IWDG->RLR = 4095;  // Máximo valor

    // Iniciar el watchdog
    IWDG->KR = 0xCCCC;  // Start IWDG
}

/**
 * @brief Configura que el watchdog timer no esté activo durante el debug.
 */
void Debug_FreezeWatchdog(void)
{
    // Congelar IWDG cuando el core está detenido en debug
    DBGMCU_APB1_FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;

    // Opcional: también congelar WWDG
    DBGMCU_APB1_FZ |= DBGMCU_APB1_FZ_DBG_WWDG_STOP;
}

/**
 * @brief Retorna la causa del reinicio del micro.
 *        Obs.: interesa especialmente identificar si el reinicio fue por el watchdog timer.
 */
ResetCause_t GetResetCause(void)
{
    ResetCause_t cause = RESET_CAUSE_UNKNOWN;
    uint32_t csr = RCC_CSR;

    // Verificar cada flag (orden de prioridad)
    if (csr & RCC_CSR_LPWRRSTF) {
        cause = RESET_CAUSE_LOW_POWER;
    }
    else if (csr & RCC_CSR_WWDGRSTF) {
        cause = RESET_CAUSE_WINDOW_WATCHDOG;
    }
    else if (csr & RCC_CSR_WDGRSTF) {
        cause = RESET_CAUSE_INDEPENDENT_WATCHDOG;
    }
    else if (csr & RCC_CSR_SFTRSTF) {
        cause = RESET_CAUSE_SOFTWARE;
    }
    else if (csr & RCC_CSR_PORRSTF) {
        cause = RESET_CAUSE_POWER_ON;
    }
    else if (csr & RCC_CSR_PADRSTF) {
        cause = RESET_CAUSE_EXTERNAL_PIN;
    }
    else if (csr & RCC_CSR_BORRSTF) {
        cause = RESET_CAUSE_BROWNOUT;
    }

    // IMPORTANTE: Limpiar los flags después de leerlos
    RCC_CSR |= RCC_CSR_RMVF;

    return cause;
}
