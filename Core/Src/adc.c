/*
 * adc.c
 *
 *  Created on: Oct 1, 2026
 *      Author: guillermo
 */

#include "main.h"

extern ADC_HandleTypeDef hadc1;

uint32_t adc_vrefint = 0;
uint32_t vdda_mV = 0;
/*
 * STM32F427:
 *
 * VREFINT_CAL:
 * ADC raw value of VREFINT measured at:
 * VDDA = 3.3 V
 * T    = 30 °C
 *
 * Address = 0x1FFF7A2A
 */
#if !defined(VREFINT_CAL_ADDR)
#define VREFINT_CAL_ADDR   ((uint16_t *)0x1FFF7A2A)
#endif
/*
 * Calibration voltage
 */
#if !defined(VREFINT_CAL_VREF)
#define VREFINT_CAL_VREF   3300UL
#endif
/**
  * @brief Read ADC1 VREFINT
  * @retval ADC raw value (0 ... 4095)
  */

uint32_t ADC_Read_VREFINT(void)
{
    uint32_t value = 0;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        value = HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);

    return value;
}


/**
  * @brief Calculate VDDA from VREFINT
  *
  * Formula:
  *
  * VDDA = VREFINT_CAL_VREF *
  *        VREFINT_CAL /
  *        VREFINT_ADC
  *
  * @param vrefint_adc ADC measured value
  * @retval VDDA in millivolts
  */
uint32_t ADC_Calculate_VDDA(uint32_t vrefint_adc)
{
    uint32_t vrefint_cal;

    /*
     * Read factory calibration value from system memory.
     */
    vrefint_cal = (uint32_t)(*VREFINT_CAL_ADDR);

    /*
     * Calculate VDDA.
     *
     * Multiplication is done first to avoid losing precision.
     */
    return (VREFINT_CAL_VREF * vrefint_cal) / vrefint_adc;
}

uint32_t Vcc()
{
  /*
   * Read VREFINT once to check that everything is working.
   */
  adc_vrefint = ADC_Read_VREFINT();

  /*
   * Calculate actual VDDA.
   */
  if (adc_vrefint != 0)
  {
    vdda_mV = ADC_Calculate_VDDA(adc_vrefint);
  }

  return vdda_mV;
}


