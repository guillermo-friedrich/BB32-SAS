/**
 ******************************************************************************
 * @file    udp_server.c
 * @author  Guillermo Friedrich
 * @version v1.1.0
 * @date    07-noviembre-2019
 * @brief   UDP server
 ******************************************************************************
 * @attention
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "cashless.h"

/* Private typedef -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Public variables ---------------------------------------------------------*/
struct udp_pcb *upcb_cli, *upcb_mgmt, *upcb_test, *upcb_lp21, *upcb_bonus, *upcb_flash;

ip_addr_t addr_serv_online={IP_SERV_ONLINE},
		      addr_serv_cashless = {IP_SERV_CASHLESS},
			  	addr_serv_mgmt = {IP_SERV_MGMT},
				  addr_serv_prog = {IP_SERV_PROG};

/* Private function prototypes -----------------------------------------------*/
static void udp_server_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Inicializa un servidor udp
 * @param  Nro de puerto
 * @return Puntero al bloque de control UDP (null en caso de error)
 */
struct udp_pcb * udp_server_init(uint16_t udp_local_port)
{
	struct udp_pcb *upcb;
	err_t err;

	/* Crea un nuevo bloque de control UDP */
	upcb = udp_new();

	if (upcb)
	{
		/* Vincula el nuevo upcb con el puerto indicado */
		/* Using IP_ADDR_ANY allow the upcb to be used by any local interface */
		err = udp_bind(upcb, IP_ADDR_ANY, udp_local_port);

		if (err == ERR_OK)
		{
			/* Setea la función callback para el nuevo upcb */
			switch(udp_local_port)
			{
			case LOCAL_PORT_MGMT:
				udp_recv(upcb, udp_mgmt_server_callback, NULL);
				break;
			case LOCAL_PORT_FLASH:
				udp_recv(upcb, udp_flash_server_callback, NULL);
				break;
			default:
				udp_recv(upcb, udp_server_callback, NULL);
				break;
			}
		} else {
			/* Error en bind() --> cierra el upcb y sale con error */
			udp_remove(upcb);
			upcb = 0;
		}
	}
	return upcb;
}

/**
 * @brief  Esta función es ejecutada cuando se recibió un datagrama UDP destinado
 *         a alguno de los puertos asignado a la misma.
 * @param  arg argumentos provistos por el usuario (udp_pcb.recv_arg)
 * @param  pcb bloque de control udp_pcb del paquete recibido
 * @param  p puntero al buffer que contiene el paquete recibido
 * @param  addr dirección IP remota desde la que proviene el datagrama recibido
 * @param  port puerto remoto desde donde proviene el datagrama recibido
 * @return None
 */
static void udp_server_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	struct pbuf *rsp;

	switch(upcb->local_port)
	{
	case LOCAL_PORT_CLI:
		if( addr->addr==features.IP_server_online && port == SERV_PORT_HELLO )
		{
		  if( flag.waitingHelloReply )
		  {
		    processHelloReply( p );
		  }
		}
		else if(addr->addr==features.IP_server_online && port==SERV_PORT_EVENTS)
		{
			processEventAck( p );
		}
		else if(addr->addr==features.IP_server_cash && (port==SERV_PORT_CASHLESS || port==SERV_PORT_PAGO_MANUAL))
		{
			processCashlessReply( p );
		}
    else if(addr->addr==features.IP_server_online && port==SERV_PORT_PAGO_MANUAL )
    {
      processCashlessReply( p );
    }
		break;
  case LOCAL_PORT_LP21:
    if( (addr->addr==features.IP_server_online || addr->addr==features.IP_server_mgmt) && port == SERV_PORT_LP21_ROM_SIGN )
    {
      if( !flag.onDemandLongPoll )
      {
        OnDemandLongPoll( addr, port, upcb, p );  // registra el pedido de LP a demanda
      }
    }
	default:
	  rsp = pbuf_alloc(PBUF_TRANSPORT, 4, PBUF_RAM);
		sprintf(rsp->payload, "ACK");
		udp_connect(upcb, addr, port);  // Conecta con el destino remoto
		udp_send(upcb, rsp);            // Envía el mensaje
		udp_disconnect(upcb);           // Libera la conexión UDP
		pbuf_free(rsp);
		break;
	}
	pbuf_free(p);  // Libera el buffer UDP recibido
}

/*
 * Funciones auxiliares varias
 */

/**
 *****************************************************************
 * @brief  Función para calcular el CRC 16 (Kermit)
 * @param  Mensaje a enviar
 * @param  Longitud del mensaje en bytes
 * @param  Valor inicial -secreto compartido entre ambos extremos-
 *         para el cálculo del CRC.
 * @return CRC calculado
 *****************************************************************
*/
uint16_t CRC16( uint8_t const message[], uint32_t nBytes, uint16_t crcval )
{
  uint16_t remainder = crcval, bit;
  uint32_t byte;
  static const uint16_t POLYNOMIAL = 0x8408;

  // Realiza la dividión modulo-2, de a un byte por vez
  for( byte = 0; byte < nBytes; ++byte )
  {
    // Coloca el siguiente byte en el resto
    remainder ^= message[byte];

    // Realiza la división modulo-2, de a un bit por vez
    for( bit = 8; bit > 0; --bit )
    {
      // Intenta dividir el bit actual
      if( remainder & 1 )
        remainder = ( remainder >> 1 ) ^ POLYNOMIAL;
      else
        remainder = ( remainder >> 1 );
    }
  }
  // El resto de la división es el CRC.

  // Lo coloca en orden Little Endian
  //remainder = remainder >> 8 | remainder << 8;

  return ( remainder );
}

/****************************************************
 * @brief  Función para cifrar / decifrar un mensaje
 * @param  Mensaje a cifrar
 * @param  Longitud del mensaje en bytes
 * @return None
 ****************************************************
*/
void cifrar( uint8_t message[], uint16_t nBytes )
{
  uint16_t i, j;

  for( i=0, j=0; i<( nBytes-2 ); ++i, j=( j+1 )%2 )
  {
    message[i] ^= message[nBytes-2+j];
  }
}

/***************************************************************************
 * @brief  Función para enviar un mensaje UDP de respuesta a otro recibido,
 *         usando el mismo buffer recibido para enviar la respuesta.
 *         No bloquea el mutex, porque está dentro del bloqueo de recepción.
 * @param  Mensaje a enviar
 * @param  Bloque de control UDP de origen
 * @param  Dirección IP de destino
 * @param  Puerto UDP de destino
 * @return None
 ***************************************************************************
*/
void sendUdpReply(struct pbuf * msg, struct udp_pcb * upcb, const ip_addr_t * dest_addr, u16_t dest_port)
{
  if( upcb )
  {
    // Conecta con el destino remoto
    udp_connect(upcb, dest_addr, dest_port);
    // Envía el mensaje
    udp_send(upcb, msg);
    // Libera la conexión UDP
    udp_disconnect(upcb);
  }
}

/****************************************************
 * @brief  Función para enviar un mensaje UDP
 * @param  Puntero al mensaje a enviar
 * @param  Longitud del mensaje a enviar
 * @param  Bloque de control del puerto de origen
 * @param  Dirección IP de destino
 * @param  Puerto UDP de destino
 * @return 1 si completó la operación
 *         0 si no
 ****************************************************
*/
int sendUDP(uint8_t * msg, int msg_len, struct udp_pcb * upcb, const ip_addr_t * dest_addr, u16_t dest_port)
{
  struct pbuf *udp_buf;

  if( !upcb )
    return 0;

  udp_buf = pbuf_alloc(PBUF_TRANSPORT, msg_len, PBUF_RAM);

  if( !udp_buf )
    return 0;

  memcpy( udp_buf->payload, msg, msg_len );

  // Conecta con el destino remoto
  udp_connect(upcb, dest_addr, dest_port);
  // Envía el mensaje
  udp_send(upcb, udp_buf);
  // Libera la conexión UDP
  udp_disconnect(upcb);

  pbuf_free( udp_buf );
  return 1;
}

