/*
 * 
 */
#ifndef __UDP_SERVER_H__
#define __UDP_SERVER_H__

/* Obs.: debe incluirse después de lwip/pbuf.h" */

/* Puertos UDP locales */
#define LOCAL_PORT_CLI          1234
#define LOCAL_PORT_MGMT         1235
#define LOCAL_PORT_LP21         1236
#define LOCAL_PORT_TEST         1204
#define LOCAL_PORT_BONUS        1210
#define LOCAL_PORT_FLASH        1237  // Obs.: para las pruebas de reprogramación de flash

/* Puertos UDP remotos */
#define SERV_PORT_HELLO              1200
#define SERV_PORT_EVENTS             1201
#define SERV_PORT_CONTADORES         1202
#define SERV_PORT_CASHLESS           (1203+10*(((MyIP() >> 16) & 0xff)%10))
#define SERV_PORT_TITO               SERV_PORT_CASHLESS
#define SERV_PORT_TEST               1204
#define SERV_PORT_MGMT               1205
#define SERV_PORT_PAGO_MANUAL        1207
#define SERV_PORT_LP21_ROM_SIGN      1208
#define SERV_PORT_MISTERY            1209
#define SERV_PORT_PROGRESIVO         1210
#define SERV_PORT_BUFFET             8000

/* Direcciones IP de servidores */
#define IPV4(n1,n2,n3,n4)   ((uint32_t)(n1 | (n2<<8) | (n3<<16) | (n4<<24)))
#define IP_SERV_ONLINE      IPV4(172,17,0,2)
#define IP_SERV_CASHLESS    IPV4(172,17,0,2)
#define IP_SERV_MGMT        IPV4(172,17,0,5)
#define IP_SERV_PROG        IPV4(0,0,0,0)     /* En caso de estar habilitado el controlador
                                                 de progresivo para un cierto grupo de
                                                 máquinas, debe pertenecer a la misma red,
                                                 y tener un 10 en el tercer octeto.
                                                 Por ej.: si la IP propia es 172.17.0.5/16,
                                                 el controlador de progresivo deberá tener
                                                 una dirección IP como 172.17.10.n */
#define IP_SERV_EXTRA       IPV4(172,17,0,9)

/* Prototipos de funciones */
struct udp_pcb * udp_server_init(uint16_t udp_port);

/***************************************************************************
 * @brief  Función para enviar un mensaje UDP de respuesta a otro recibido,
 *         usando el mismo buffer recibido para enviar la respuesta.
 *         No bloquea el mutex, porque está dentro del bloqueo de recepción.
 *         Luego de transmitir libera el buffer pBuf.
 * @param  Mensaje a enviar
 * @param  Bloque de control UDP de origen
 * @param  Dirección IP de destino
 * @param  Puerto UDP de destino
 * @return None
 ***************************************************************************
*/
void sendUdpReply(struct pbuf * msg, struct udp_pcb * upcb, const ip_addr_t * dest_addr, uint16_t dest_port);

/****************************************************
 * @brief  Función para enviar un mensaje UDP
 * @param  Puntero al mensaje a enviar
 * @param  Longitud del mensaje a enviar
 * @param  Bloque de control del puerto de origen
 * @param  Dirección IP de destino
 * @param  Puerto UDP de destino
 * @return None
 ****************************************************
*/
int sendUDP(uint8_t * msg, int msg_len, struct udp_pcb * upcb, const ip_addr_t * dest_addr, uint16_t dest_port);

uint16_t CRC16 (uint8_t const message[], uint32_t nBytes, uint16_t crcval);

void cifrar (uint8_t message[], uint16_t nBytes);

#define decifrar(m,n) cifrar(m,n)

/* Bloques de control UDP de los distintos puertos locales */
extern struct udp_pcb *upcb_cli, *upcb_mgmt, *upcb_test, *upcb_lp21, *upcb_bonus, *upcb_flash;

/* Direcciones IP de los distintos servidores */
extern ip_addr_t addr_serv_online, addr_serv_cashless, addr_serv_mgmt, addr_serv_prog, addr_serv_extra;

#endif /* __UDP_SERVER_H */
