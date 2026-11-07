/*
 * mgmt.c
 *
 *  Created on: 25 nov. 2019
 *      Author: guillermo
 */

/* Includes ------------------------------------------------------------------*/
//#include "cashless.h"   // Este incluye a todos los demás *.h

//#include "main.h"
//#include "stdint.h"
//#include "lwip.h"
//#include "lwipopts.h"
//#include "lwip/netif.h"
//#include "lwip/timeouts.h"
//#include "lwip/udp.h"
//#include "lwip/pbuf.h"
//#include "lwip/ip_addr.h"
//#include "lwip/ip.h"
//#include "mgmt.h"
//#include "udp_server.h"
//#include "cashless.h"

#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "udp.h"
#include <string.h>
#include "lcd.h"
#include "udp_server.h"
#include "mgmt.h"
#include "flash.h"
#include "adc.h"
#include "cashless.h"

FeaturesType features;

uint32_t myCfgIP;

extern struct netif gnetif;

/* Defines locales -----------------------------------------------------------*/

/* Macros locales ------------------------------------------------------------*/

/* Prototipos de funciones locales -------------------------------------------*/
//static void updateIP_MAC(uint8_t newIP[]);

/* Funciones -----------------------------------------------------------------*/

/**
 * @brief Esta función es ejecutada cuando se recibe un datagrama UDP destinado
 *        al puerto de administración.
 * @param arg argumentos provistos por el usuario (udp_pcb.recv_arg)
 * @param pcb bloque de control udp_pcb del paquete recibido
 * @param p puntero al buffer que contiene el paquete recibido
 * @param addr dirección IP remota desde la que proviene el datagrama recibido
 * @param port puerto remoto desde donde proviene el datagrama recibido
 * @return None
 */
void udp_mgmt_server_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
//struct pbuf *rsp;
	MgmtMsgType *msg = p->payload;

	if ( (addr->addr==addr_serv_online.addr || addr->addr==addr_serv_mgmt.addr) && !CRC16((uint8_t *) msg, p->len, MGMT_CRC_SEED) )
	{
		switch (msg->msg1.op)
		{
		case MGMT_GET_VERSION:
			// Responde con la versión que tiene instalada
			if (p->len == sizeof(msg->msg1))
			{
				msg->msg1.op = MGMT_SEND_VERSION;
				strcpy(msg->msg1.version, VERSION);
				msg->msg1.crc = CRC16((uint8_t *) msg, sizeof(msg->msg1) - 2, MGMT_CRC_SEED);
				sendUdpReply( p, upcb, addr, port );
			}
			break;

    case MGMT_GET_FEATURES:
      // Responde con las features que tiene configurada
      if (p->len == sizeof(msg->msg3))
      {
        msg->msg3.op = MGMT_SEND_FEATURES;
        msg->msg3.features = features;
        msg->msg3.crc = CRC16((uint8_t *) msg, sizeof(msg->msg3) - 2, MGMT_CRC_SEED);
        sendUdpReply( p, upcb, addr, port );
      }
      break;

    case MGMT_SET_IP_MAC:
	    // Actualiza IP del módulo y la almacena en la flash
	    if( p->len == sizeof(msg->msg1) )
	    {
	      if( Vcc() < 3000 )
	        display_low_vcc();
	      else
	      {
	        //updateIP_MAC(msg->msg1.IP);
          myCfgIP = IPV4(msg->msg1.IP[0], msg->msg1.IP[1], msg->msg1.IP[2], msg->msg1.IP[3]);
	        gnetif.ip_addr.addr = myCfgIP;
	        netif_set_addr(&gnetif, &gnetif.ip_addr, &gnetif.netmask, &gnetif.gw);

	        if( saveConfigFlash( &features, IPV4(msg->msg1.IP[0], msg->msg1.IP[1], msg->msg1.IP[2], msg->msg1.IP[3]) ) )
          {
            LCD_print("ERROR GRABANDO","CONFIG EN FLASH");
          }else{
            LCD_print("SE HA GRABADO","CONFIG EN FLASH");
          }
        }
	    }
	    break;

	  case MGMT_SET_FEATURES:
	    // Recibió un conjunto de nuevas features a configurar en el módulo
      if (p->len == sizeof(msg->msg3))
	    {
	      if( Vcc() < 3000 )
	      {
	        display_low_vcc();
	      }
	      else
	      {
          // Fuerza a configurar cashless habilitado y tito deshabilitado
          msg->msg3.features.cashless |= CASHLESS_ENABLED;
          msg->msg3.features.tito = TITO_DISABLED;

	        if( saveConfigFlash( &msg->msg3.features, MyIP()) )
	        {
	          LCD_print("ERROR GRABANDO","CONFIG EN FLASH");
	        }else{
	          LCD_print("SE HA GRABADO","CONFIG EN FLASH");

	          // Reinicia el módulo para que tengan efecto los cambios
	          // Se queda en un loop esperando a que el WDT haga resetear al micro.
	          startWatchdog(_1seg);
	          while(1);
	        }
	      }
	    }
	    break;

    case MGMT_SET_IP_FEATURES:
      // Recibió una nueva IP y un conjunto de nuevas features a configurar en el módulo
      if (p->len == sizeof(msg->msg5))
      {
        if( Vcc() < 3000 )
        {
          display_low_vcc();
        }
        else
        {
          // Fuerza a configurar cashless habilitado y tito deshabilitado
          msg->msg5.features.cashless |= CASHLESS_ENABLED;
          msg->msg5.features.tito = TITO_DISABLED;

          if( saveConfigFlash( &msg->msg5.features, IPV4(msg->msg5.IP[0], msg->msg5.IP[1], msg->msg5.IP[2], msg->msg5.IP[3]) ) )
          {
            LCD_print("ERROR GRABANDO","CONFIG EN FLASH");
          }else{
            LCD_print("SE HA GRABADO","CONFIG EN FLASH");

            // Reinicia el módulo para que tengan efecto los cambios
            // Se queda en un loop esperando a que el WDT haga resetear al micro.
            startWatchdog(_1seg);
            while(1);
          }
        }
      }
      break;

	  case MGMT_DELE_FEATURES:
	    // Recibió la orden de borrar las features y volver a los valores por defecto
	    if( Vcc() < 3000 )
	    {
	      display_low_vcc();
	    }
	    else
	    {
	      if( deleteConfigFlash() )
	      {
	        LCD_print("ERROR BORRADO","CONFIG EN FLASH");
	      }else{
          LCD_print("SE HA BORRADO","CONFIG EN FLASH");
	      }
	    }
	    break;

	  case MGMT_RESET:
	    /* Recibió la orden de reiniciar el micro. Activa el watchdog y se queda
	     * en un loop esperando a que el WDT haga resetear al micro */
	    startWatchdog(_1seg);
	    while(1) {}

		default:
			break;
		}
	}
	pbuf_free(p);
}
#if 0
/**
 * @brief Actualiza IP y MAC del módulo y reinicia el Controlador Ethernet
 * @param newIP Nueva dirección IP (4 bytes)
 * @return None
 */
static void updateIP_MAC(uint8_t newIP[])
{
	struct netif *eth_netif = netif_find("st0");
#if MAC_ADDRESS_DS2401 == 0
	int i;
#endif

	/* Obs.: por ahora solo modifica las direcciones IP y MAC en uso, pero no las almacena
	 * para ser leídas luego de un reinicio. Falta definir la manera de organizar el
	 * almacenamiento de los parámetros de configuración del módulo en memoria flash.
	 */

	if (eth_netif)
	{
		eth_netif->ip_addr.addr = IPV4(newIP[0], newIP[1], newIP[2], newIP[3]);

#if MAC_ADDRESS_DS2401 == 0
		// El módulo no tiene una MAC única, por lo que genera
		// una nueva en base a la IP que se le asignó.
		eth_netif->hwaddr[0] = 0x02;
		eth_netif->hwaddr[1] = CRC16(newIP,4,0) % 256;
		for (i = 0; i < 4; ++i)
		{
			eth_netif->hwaddr[2 + i] = newIP[i];
		}
		//ETH_MACAddressConfig(ETH_MAC_Address0, eth_netif->hwaddr);
#endif
	}
}
#endif

/**
 * @brief Graba un nuevo valor de UID
 */
void setUID(uint16_t uid)
{
  features.uid = uid;

  if( saveConfigFlash( &features, MyIP()) )
    LCD_print("ERROR GRABANDO","CONFIG EN FLASH");
  else{
    LCD_print("SE HA GRABADO","CONFIG EN FLASH");
  }
}

/**
 * @brief Graba una nueva dirección IP del servidor de Mistery
 */
void setIpMistery(uint8_t newIP[])
{

  features.IP_server_prog = IPV4(newIP[0], newIP[1], newIP[2], newIP[3]);

  if( saveConfigFlash( &features, MyIP()) )
    LCD_print("ERROR GRABANDO","CONFIG EN FLASH");
  else{
    LCD_print("SE HA GRABADO","CONFIG EN FLASH");
  }
}

/**
 * @brief Retorna la dirección IP propia
 */
uint32_t MyIP()
{
	struct netif *eth_netif = netif_find("st0");
	if (eth_netif)
		return eth_netif->ip_addr.addr;
	return 0;
}

/**
 * @brief Presenta la tensión de alimentación en el display
 */
void display_vcc()
{
  char s[17];
  uint16_t v = Vcc();

  sprintf( s, "%u.%u", v/1000, v%1000 );
  LCD_print1(s);
}

/**
 * @brief Presenta un mensaje indicando baja tensión de alimentación
 */
void display_low_vcc()
{
  char s[17];
  uint16_t v = Vcc();
  sprintf(s, "Vcc = %-u.%03u V", v/1000, v%1000);
  LCD_print("BAJA TENSION",s);
}



