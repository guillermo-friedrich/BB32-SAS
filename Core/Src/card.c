  /*
 * card.c
 *
 *  Created on: 28 nov. 2019
 *      Author: guillermo
 */

#include "cashless.h"

// Variables globales

volatile uint32_t delayTicksCard = 0; /* Contador de ticks usado para generar los
                                         temporizados para el manejo de la tarjeta chip */

uint8_t cardID[32];   // ID de cliente o vendedor grabado en la tarjeta

// Constantes y macros locales

// Comandos para la tarjeta

#define CMD_READ_MAIN  0x30     // Leer memoria ppal
#define CMD_WRITE_MAIN 0x38     // Escribir en memoria ppal
#define CMD_READ_SECU  0x31     // Leer memoria de seguridad
#define CMD_WRITE_SECU 0x39     // Escribir en memoria de seguridad
#define CMD_COMP_VERIF 0x33     // Comparar datos de verificación

// Macros para manejar las operaciones con la tarjeta

#define RESET_H()   HAL_GPIO_WritePin(GPIOD,GPIO_PIN_8,GPIO_PIN_SET)
#define RESET_L()   HAL_GPIO_WritePin(GPIOD,GPIO_PIN_8,GPIO_PIN_RESET)
#define CLK_H()     HAL_GPIO_WritePin(GPIOD,GPIO_PIN_7,GPIO_PIN_SET)
#define CLK_L()     HAL_GPIO_WritePin(GPIOD,GPIO_PIN_7,GPIO_PIN_RESET)
#define READ()      (HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_9)?0:1)
#define WRITE0()    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_6,GPIO_PIN_SET)
#define WRITE1()    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_6,GPIO_PIN_RESET)

//#ifdef PRUEBA_OFFLINE
//// Usa el pulsador USER para simular la tarjeta insertada
//#define CARD_IN()   GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)
//#else
#define CARD_IN()   HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_4)
//#endif

/* Prototipos locales */
static int cardAnswerToReset(uint8_t * cardID);
static void cardBreak(void);
int cardRead(uint8_t cmd, uint8_t *buf, uint8_t from, uint8_t nBytes);

/**
 * @brief Retorna el estado del lector de tarjetas chip
 * @param None
 * @retval 0 --> no hay tarjeta / 1 --> hay una tarjeta colocada en el lector
 */
int getCardStatus()
{
  // CARD_IN es normalmente abierto, se pone a uno cuando hay una tarjeta presente
  if(CARD_IN())
    return 1;         // Tarjeta presente
  else
    return 0;         // No hay tarjeta presente en el lector
}

/**
 * @brief  Verifica el estado del lector de tarjetas chip.
 *         Si una tarjeta fue insertada obtiene sus datos, registra sus datos para el pedido
 *         de cash. Si fue retirada, registra el evento para descargar créditos de la VGM.
 *         Obs.: esta función lee 32 bytes de la tarjeta, a partir de la dirección 32. Si se
 *         quisiera escribir en ese rango de direcciones, valen los siguientes ejemplos:
 *         cardWrite("NOMBRE Y APELLIDO - Nro de Documento", 32, 32);
 * @param  None
 * @return None
 */
void checkCardReader()
{
  if( getCardStatus() != flag.cardStatus )
	{
		flag.cardStatus = !flag.cardStatus;
		if( flag.cardStatus )
		{
			if ( cardAnswerToReset(cardID) )
			{
			  if( cardRead(CMD_READ_MAIN, cardID, 32, 32) )
			    cash_flag = DO_CASHIN;
			}
		}else{
			cash_flag = DO_CASHOUT;
		}
	}
}

/**
 * @brief Efectúa el reset y answer-to-reset de la tarjeta.
 * @param[out] cardID Deposita los 32 bits leidos en cardID
 * @return 1 si tuvo éxito, 0 si falló la operación
 */
static int cardAnswerToReset(uint8_t * cardID)
{
  int i,j;

  if( getCardStatus() )
  {
    RESET_H();
    HAL_Delay(2);        // t10: mín 4 us
    CLK_H();
    HAL_Delay(2);        // tR + t15 : mín 10 us
    CLK_L();
    HAL_Delay(2);        // t11: mín 4 us
    RESET_L();
    HAL_Delay(2);        // t14: mín 4 us
    for(i=0;i<4;++i)
    {
      cardID[i]=0;

      for(j=0; j<8; ++j)
      {
        if( READ() )
          cardID[i] |= (1 << j);
        CLK_H();
        HAL_Delay(2);     // tR + t15: mín 10 us
        CLK_L();
        HAL_Delay(2);     // tF + t16: mín 10 us
      }
    }
  }
  return getCardStatus();
}

/**
 * @brief Hace un break para interrumpir una secuencia
 */
static void cardBreak()
{
	CLK_L();
	HAL_Delay(2);
	RESET_H();
	HAL_Delay(2);        // t18: mín 5 us
	RESET_L();
	HAL_Delay(2);
}

/**
 * @brief Lee "nBytes" de la memoria indicada, a partir de la dirección "from"
 * @param[in]  cmd Región de memoria a leer
 * @param[out] buf Puntero al buffer donde se deben depositar los bytes leídos de la memoria
 * @param[in]  from Dirección inicial
 * @param[in]  nBytes Cantidad de bytes a leer
 */
int cardRead(uint8_t cmd, uint8_t *buf, uint8_t from, uint8_t nBytes)
{
	int i, j;

	if (getCardStatus())
	{
		WRITE1();
		CLK_H();
		HAL_Delay(2);        // tR + t15: mín 10 us
		WRITE0();
		HAL_Delay(2);        // tF + t16: mín 10 us
		CLK_L();

		// Envía comando
		for (i = 0; i < 8; ++i)
		{
			if (cmd & (1 << i))
				WRITE1();
			else
				WRITE0();
			HAL_Delay(2);        // t4: mín 1 us
			CLK_H();
			HAL_Delay(2);        // tR + t15: mín 10 us
			CLK_L();
			/* Debería ir tF + t16, pero como en la práctica es el mismo tiempo que
			 * se implementó para t4, no se lo aplica
			HAL_Delay(2);        // tR + t15: mín 10 us
			*/
		}

		// Envía dirección inicial
		for (i = 0; i < 8; ++i)
		{
			if (from & (1 << i))
				WRITE1();
			else
				WRITE0();
			HAL_Delay(2);        // t4: mín 1 us
			CLK_H();
			HAL_Delay(2);        // tR + t15: mín 10 us
			CLK_L();
			/* Debería ir tF + t16, pero como en la práctica es el mismo tiempo que
			 * se implementó para t4, no se lo aplica
			HAL_Delay(2);        // tR + t15: mín 10 us
			*/
		}

		// Envía 3er byte (no importa)
		for (i = 0; i < 8; ++i)
		{
			WRITE0();
			HAL_Delay(2);        // t4: mín 1 us
			CLK_H();
			HAL_Delay(2);        // tR + t15: mín 10 us
			CLK_L();
			/* Debería ir tF + t16, pero como en la práctica es el mismo tiempo que
			 * se implementó para t4, no se lo aplica
			HAL_Delay(2);        // tR + t15: mín 10 us
			*/
		}

		HAL_Delay(2);        // tF + t16: mín 10 us
		CLK_H();
		HAL_Delay(2);        // tR + t15: mín 10 us
		WRITE1();
		HAL_Delay(2);        // tiempo adicional, quizás no sea necesario
		CLK_L();
		HAL_Delay(2);        /* t7: máx 2.5 us hasta que comienzan a salir los datos.
		                         esta espera adicional no afecta, sino lo contrario */

		for (i = 0; i < nBytes; ++i)
		{
			buf[i] = 0;

			for (j = 0; j < 8; ++j)
			{
				if ( READ() )
					buf[i] |= (1 << j);
				CLK_H();
				HAL_Delay(2);        // tR + t15: mín 10 us
				CLK_L();
				HAL_Delay(2);        // tF + t16: mín 10 us
			}
		}
	}

	cardBreak();

	return getCardStatus();
}
