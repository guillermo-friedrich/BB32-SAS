/*
 * cashless.h
 *
 *  Created on: 27 nov. 2019
 *      Author: guillermo
 */

#ifndef INC_CASHLESS_H_
#define INC_CASHLESS_H_

// Versión del firmware
#define VERSION "BB32.9.71n"

/* Includes ------------------------------------------------------------------*/
//#include <stdint.h>
//#include "pbuf.h"
//#include "mgmt.h"
//#include "eventos.h"

#include "main.h"
#include "lwip.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "udp.h"
#include "pbuf.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ds2401.h"
#include "udp_echo.h"
#include "sas.h"
#include "lcd.h"
#include "card.h"
#include "cashless.h"
#include "uart.h"
#include "eventos.h"
#include "longpoll.h"
#include "udp_server.h"
#include "tito.h"
#include "mgmt.h"
#include "flash.h"
#include "hello.h"
#include "aft.h"
#include "watchdog.h"

/* Macros y constantes */

/* Referencia a los leds por color */
#define LED_RED     LED5
#define LED_BLUE    LED6
#define LED_GREEN   LED4
#define LED_ORANGE  LED3

#define userButtonPressed() GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)

/* Constantes que definen tiempos (en base al SysTick de 1 ms) */

#define _10ms   10
#define _20ms   20
#define _30ms   30
#define _40ms   40
#define _50ms   50
#define _100ms  100
#define _110ms  110
#define _120ms  120
#define _150ms  150
#define _200ms  200
#define _210ms  210
#define _250ms  250
#define _300ms  300
#define _400ms  400
#define _500ms  500
#define _800ms  800
#define _1seg   100
#define _1500ms 1500
#define _2seg   2000
#define _3seg   3000
#define _4seg   4000
#define _5seg   5000
#define _6seg   6000
#define _10seg  10000
#define _12seg  12000
#define _15seg  15000
#define _20seg  20000
#define _30seg  30000
#define _1min   60000
#define _2min   120000

/* Máscaras de bits */
#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80

#if 1
// EFT no implementado --> sólo define este para un mensaje de respuesta
#define EFT_SUCCESS                     0x00 // Operation successfully completed
#else
//----------------------------------------------------------
// Códigos de transfer status de EFT, reportados por la VGM
//----------------------------------------------------------

#define EFT_SUCCESS                     0x00 // Operation successfully completed
#define EFT_DOOR_OPEN                   0x02 // Gaming machine door open
#define EFT_EXCEEDED_AMOUNT             0x04 // Transfer amount exceeded gaming machine credit limit
#define EFT_INVALID_TRANSACTION_NUMBER  0x05 // Invalid transaction number
#define EFT_NO_CREDIT_SWITCH            0x06 // Gaming machine does not have a credit switch
#define EFT_VGM_TILT                    0x07 // Gaming machine is in a tilt condition
#define EFT_INVALID_ACK                 0x08 // Invalid ACK message received
#define EFT_VGM_PLAYING                 0x09 // Gaming machine is in game play mode
#define EFT_NON_BCD_DATA                0x0A // Data field contains non-BCD data
#define EFT_TRANSFER_ALREADY_COMPLETED  0x0B // Host transfer request has already been completed
#define EFT_VGM_DISABLED                0x0C // Gaming machine disabled
#define EFT_VGM_OUT_OF_SERVICE          0x0D // Gaming machine out of service
#define EFT_VGM_BUSY                    0x0E // Gaming machine busy

// El siguiente no es un código de estado EFT, pero se lo define para
// reportar que no se recibe respuesta a un comando EFT
#define EFT_NO_RESPONSE                 0xE1 // (igual a AFT_NO_RESPONSE)
// Idem para reportar una respuesta errónea a un comando EFT
#define EFT_BAD_RESPONSE                0xE2 // (igual a AFT_BAD_RESPONSE)
// Idem para reportar una transacción abortada por el host (por
// ejemplo, por inconsistencia en el mensaje de ACK del servidor)
#define EFT_TRANSFER_CANCELLED_BY_HOST  0x80 // (igual a AFT_TRANSFER_CANCELLED_BY_HOST)
// Idem para reportar error porque no se recibió respuesta del servidor
#define EFT_SERVER_NO_RESPONSE          0xE3 // (igual a AFT_SERVER_NO_RESPONSE)
#endif

#endif /* INC_CASHLESS_H_ */
