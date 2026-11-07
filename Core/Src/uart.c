/*
 * @file   uart0.c
 * @date   2 dic. 2019
 * @author Guillermo
 * @brief  Manejo de la UART usada para la comunicación mediante protocolo SAS.
 *         Se debe definir sobre que UART física se implementa la comunicación,
 *         pudiendo ser UART4 (RS232), UART5 (RS232) o UART8 (RS485)
 */
#include "stdint.h"
#include "main.h"
#include "cashless.h"
#include "uart.h"

extern UART_HandleTypeDef huart5;

#if 1

// Buffers para la transmisión y recepción con la VGM
uint8_t tx_buffer[128];
int tx_buflen=0;               // cantidad de bytes almacenados en tx_buffer

volatile int rx_buflen;
uint8_t rx_buffer[128];

static uint8_t rx_dma_buffer[128];

void initUART0 (void)
{

}

void UART0_start_rx()
{
  __HAL_UART_CLEAR_IDLEFLAG(&huart5);           // limpiar IDLE pendiente
  HAL_UART_AbortReceive(&huart5);               // asegurar que no hay RX en curso
  HAL_UARTEx_ReceiveToIdle_DMA(&huart5, (uint8_t *)rx_dma_buffer, sizeof(rx_dma_buffer));
  rx_buflen = 0;
}

void UART0_tx( uint8_t *msg, uint32_t msglen )
{
  int i;

  // Antes de iniciar la transmisión prepara la recepción por DMA
  UART0_start_rx();

  if( msglen > 0 )
  {
    // Envía dirección con wakeup
    UART0_tx_addr( msg[0] );

    // Y luego el resto del mensaje
    for(i=1; i<msglen; ++i)
      UART0_tx_data( msg[i] );
  }
}

void UART0_tx_addr ( uint8_t addr )
{
  uint16_t data16 = (uint16_t)addr | 0x100;

  HAL_UART_Transmit(&huart5, (uint8_t *)&data16, 1, HAL_MAX_DELAY);
}

void UART0_tx_data ( uint8_t data8 )
{
  uint16_t data16 = (uint16_t)data8;

  HAL_UART_Transmit(&huart5, (uint8_t *)&data16, 1, HAL_MAX_DELAY);
}

int UART0_rx ()
{
  uint32_t t0;

  HAL_UARTEx_ReceiveToIdle_DMA(&huart5, (uint8_t *)rx_dma_buffer, sizeof(rx_dma_buffer));

  rx_buflen = 0;

  // Espera una trama o parte de ella
  t0 = HAL_GetTick();

  while( (HAL_GetTick() - t0) < (rx_buflen?_30ms:_500ms) );

  return rx_buflen;
}

/**
  * @brief  Reception Event Callback (Rx event notification called after use of advanced reception service).
  * @param  huart UART handle
  * @param  Size  Number of data available in application reception buffer (indicates a position in
  *               reception buffer until which, data are available)
  * @retval None
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
  UNUSED(Size);

  int i;

  if (huart->Instance == UART5 && HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_IDLE)
  {
    /* Size = bytes recibidos en esta trama */

    for (i = 0; i < Size; i++)
    {
      if( i < sizeof(rx_buffer) )
        rx_buffer[i] = rx_dma_buffer[i];
    }
    rx_buflen = Size;

    /* Reactivar recepción */
    //HAL_UARTEx_ReceiveToIdle_DMA(&huart5, (uint8_t*)rx_dma_buffer, sizeof(rx_dma_buffer));
  }
}

#else
#include "stm32f4xx.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_rcc.h"
#include "misc.h"
#include "cashless.h"

// Define que UART usa para la comunicación con protocolo SAS
// Las opciones son: UART4 (RS232), UART5 (RS232), UART8 (RS485)
#define UART0            UART5
#define UART0_IRQHandler UART5_IRQHandler

#ifndef UART0
#define UART0            UART4
#define UART0_IRQHandler UART4_IRQHandler
#endif

/**
 * @brief  Inicializa la UART4 a 19200-8-N-1, para la comunicación con la VGM usando SAS
 * @param  None
 * @retval None
 */

void initUART0()
{
  USART_InitTypeDef UART_InitStruct;
  GPIO_InitTypeDef GPIO_InitStruct;
  NVIC_InitTypeDef NVIC_InitStruct;
  int UART0_IRQn;

  // Configura elementos comunes para cualquiera de las UART
  GPIO_StructInit(&GPIO_InitStruct);
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
  GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;

  if( UART0 == UART4 )
  {
    //Inicializa PC10 y PC11 (UART4)
    RCC_AHB1PeriphClockCmd(RCC_AHB1ENR_GPIOCEN, ENABLE);

    //Configura AF UART4
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_UART4);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_UART4);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

//  USART_ClockStructInit(&UART4_ClockInitStruct);
//  USART_ClockInit(UART4, &UART4_ClockInitStruct);

    /* NVIC configuration */
    /* Configure the Priority Group to 2 bits */
//  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    UART0_IRQn = UART4_IRQn;
  }
  else if( UART0 == UART5 )
  {
    //Inicializa PC12 y PD2 (UART5)
    RCC_AHB1PeriphClockCmd(RCC_AHB1ENR_GPIOCEN, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1ENR_GPIODEN, ENABLE);

    //Configura AF UART5
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_UART5);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource2,  GPIO_AF_UART5);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);

    UART0_IRQn = UART5_IRQn;
  }
  else if ( UART0 == UART8 )
  {
    //Inicializa PE0 y PE1 (UART8)
    RCC_AHB1PeriphClockCmd(RCC_AHB1ENR_GPIOEEN, ENABLE);

    //Configura AF UART8
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource0, GPIO_AF_UART8);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource1, GPIO_AF_UART8);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOE, &GPIO_InitStruct);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART8, ENABLE);

    UART0_IRQn = UART8_IRQn;
  }else{
    return;
  }

  USART_StructInit(&UART_InitStruct);
  UART_InitStruct.USART_WordLength = USART_WordLength_9b;
  UART_InitStruct.USART_BaudRate = 19200;
  USART_Init(UART0, &UART_InitStruct);

  /* Enable the UART0 Interrupt */
  NVIC_InitStruct.NVIC_IRQChannel = UART0_IRQn;
  NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 15;  // OJO: revisar el nivel de prioridad
  NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStruct);

  NVIC_SetPriority(UART0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);

  /* Habilita interrupciones por recepción para la UART seleccionada */
  USART_ITConfig(UART0,USART_IT_RXNE,ENABLE);

  NVIC_EnableIRQ(UART0_IRQn);

  /* Habilita la UART0 (SAS) */
  USART_Cmd(UART0,ENABLE);
}

/**
 * @brief  Rutina de interrupción de la UART4/5/8
 *         Obs.: descomentar la que corresponda, de acuerdo a la UART usada
 */
void UART0_IRQHandler()
{
  uint16_t dr;
  BaseType_t xHigherPriorityTaskWoken=pdFALSE;

  if(USART_GetITStatus(UART0, USART_IT_RXNE))
  {
    dr=USART_ReceiveData(UART0);
    if( rx_buflen < 128 )
      rx_buffer[rx_buflen++] = (unsigned char) dr;

    xSemaphoreGiveFromISR(xSemaphoreUART0, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
  }
}

/**
 * @brief  Función para enviar un byte por la UART0 (SAS)
 *         (Si wake != 0 --> transmite el byte con el 9no bit en '1')
 * @param  data Byte a transmitir
 * @param  wake Flag para indicar si debe transmitir el bit de wakeup en '1'
 * @retval None
 */
void UART0_tx( uint16_t data, uint16_t wake )
{
  /* Espera que haya terminado de enviar el byte anterior */
  while( !USART_GetFlagStatus(UART0, USART_FLAG_TXE) );

  if(wake)
    USART_SendData(UART0, data | 0x0100);
  else
    USART_SendData(UART0, data);
}

/**
 * @brief  Función para esperar la recepción de un mensaje por la UART4
 * @param  Máxima cantidad de bytes a esperar
 * @retval Cantidad de bytes recibidos (0: no se recibió nada)
 */
int UART0_rx(int maxlen)
{
  rx_buflen = 0;

  for(rx_buflen=0; rx_buflen<maxlen; )
  {
    /* El primer byte puede tener una demora mayor que los demás. Los peores casos
     * registrados son: 2 s para GP y LP en EIBE;
     *                  1 s para AFT en EIBE;
     *                  500 ms para AFT en otras máquinas (no EIBE)
     *                  300 ms para GP y LP en otras máquinas (no EIBE)
     * Obs.: por ahora lo dejamos en el peor caso, 2 s, para todos los tipos de mensajes
     */

    if( xSemaphoreTake(xSemaphoreUART0, (rx_buflen==0?_2seg:_20ms)) == pdFALSE )
    {
      /* Salió por timeout de la espera del semáforo --> fin de la recepción */
      break;
    }
  }
  return rx_buflen;
}
#endif
