/*
 * tito.c
 *
 *  Created on: 16 ene. 2020
 *      Author: guillermo
 */
//-------------------------------------------------------
// tito.c
// Funciones para el manejo de tickets (TITO)
//-------------------------------------------------------

#include "main.h"
#include "lwip.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "udp.h"
#include "ds2401.h"
#include "udp_echo.h"
#include <stdio.h>
#include <string.h>
#include "sas.h"
#include "lcd.h"
#include "card.h"
#include "cashless.h"
#include "uart.h"
#include "eventos.h"
#include "longpoll.h"
#include "udp_server.h"
#include "mgmt.h"
#include "tito.h"

uint8_t ticketID[9];        // Para almacenar el código de validación de un ticket
uint32_t montoMaxTicket;    // Monto máximo que puede tener un ticket con validacion system

#define SWAPB(n) (((n>>8)&0xff) | ((n<<8)&0xff00))

//--------------------------------------------------------
// TITO - Procesamiento de un ticket emitido por la VGM
// Se envían los datos del ticket al servidor
// Obs.: si se ejecuta a partir de un evento 3E (pago
// manual) se lo trata igual pero se envía en errorCode
// un 3E para que el servidor sepa que puede no existir
// un ticket (algunas máquinas imprimen ticket por un pago
// manual y otras no)
//--------------------------------------------------------

#define pr ((EnhancedValidationInfoType *)rx_buffer)

void ticket_out()
{
  uint16_t i, j;

  // Envía un LP 4D para obtener los datos y monto del ticket
  tx_buflen = 5;
  tx_buffer[0] = vgmAddr;
  tx_buffer[1] = 0x4D;
  tx_buffer[2] = 0x00;
  i = CRC16(tx_buffer, tx_buflen-2, 0);
  tx_buffer[3] = i & 0x00ff;
  tx_buffer[4] = (i >> 8) & 0x00ff;

  // Si no recibe respuesta al LP 4D, intenta el envío hasta 3 veces
  for(j=0; j<3; ++j)
  {
    UART0_tx(tx_buffer, tx_buflen);

    if( UART0_rx() && !CRC16(rx_buffer,rx_buflen,0) )
      break;
  }

  // Envía 0x80 y espera durante el período entre pollings
  delay( pollingPeriod );
  UART0_tx_80();
  delay( pollingPeriod );

  if( j > 2 )
    // No tuvo respuesta --> retorna para intentar más tarde
    return;

  flag.validationSystemOK = 0;  // En validación system, para el próximo ticket debe generar un nuevo validation number

  // Resetea el flag de requerimiento de TITO
  *((uint32_t *)&ticket_flag) = 0;
}

#undef pr

//--------------------------------------------------------------
// TITO - Setea validation ID para el modo de validación segura
//--------------------------------------------------------------

#define pt ((SetSecureEnhancedValidationIDType *)tx_buffer)
#define pr ((SetSecureEnhancedValidationIDType *)rx_buffer)
#define buf rx_buffer

void set_validation_id()
{
  uint16_t i, j;

  // calcula un número a partir de la IP, para usar como machine IP, que
  // permita distinguir una máquina de otra con una cierta dispersión
  buf[0] = MyIP()>>16;
  buf[1] = MyIP()>>24;
  buf[2] = ~(MyIP()>>16);
  buf[3] = ~(MyIP()>>24);
  buf[4] = (uint8_t)tiempo.anio;
  buf[5] = tiempo.mes;
  buf[6] = tiempo.dia;
  buf[7] = tiempo.hora;
  buf[8] = tiempo.min;
  buf[9] = tiempo.seg;
  i = CRC16(buf, 10, 0);
  j = CRC16(buf, 10, 0x1234);

  // Envía un LP 4C para setear el validation ID
  // que necesita la validación enhanced

  tx_buflen = sizeof(SetSecureEnhancedValidationIDType);

  pt->vgmAddr = vgmAddr;
  pt->command = 0x4C;

  pt->machine_id[0] = i;
  pt->machine_id[1] = i>>8;
  pt->machine_id[2] = j ^ (j>>8);

  pt->sequence_number[0] = (uint8_t)(tiempo.anio<<4 | tiempo.mes);
  pt->sequence_number[1] = tiempo.dia<<4 | tiempo.hora;
  pt->sequence_number[2] = tiempo.min<<4 | tiempo.seg;

  pt->crc = CRC16((uint8_t *)pt, sizeof(SetSecureEnhancedValidationIDType)-2, 0);

  // Si no recibe respuesta al LP 4C, intenta el envìo hasta 3 veces
  for(j=0; j<3; ++j)
  {
    UART0_tx(tx_buffer, tx_buflen);

    if( UART0_rx() && !CRC16(rx_buffer,rx_buflen,0) )
      break;
  }

  // Envía 0x80 y espera durante el período entre pollings
  delay( pollingPeriod );
  UART0_tx_80();
  delay( pollingPeriod );

  // Resetea el flag de requerimiento de TITO
  *((uint32_t *)&ticket_flag) = 0;
}

#undef buf
#undef pt
#undef pr

