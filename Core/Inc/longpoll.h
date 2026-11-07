/*
 * longpoll.h
 *
 * Archivo de encabezado de longpoll.c
 *
 *  Created on: May 28, 2024
 *      Author: guillermo
 */

#ifndef INC_LONGPOLL_H_
#define INC_LONGPOLL_H_

// Estructura del mensaje en que se se reportan los resultados de los LONG POLLS
// Si alguno de los LP no respondió, el arreglo correspondiente se envía lleno con 0xFF,
// excepto lp1f y lp54, que se llenan con 0x00

typedef struct __attribute__((packed))
{
  char msgID[2];         // Id. del mensaje de Long Polls --> "LP"
                         // Contadores de longitud fija (LP standard)
  uint8_t lpa0[3];       // Enabled features: features 1, 2 y 3 (para game 0)
  uint8_t lp1f[5];       // denomination (1 byte) - base percentage (4 bytes)
  uint8_t lp54[4];       // SAS versión (3 bytes ASCII + null)
  uint8_t lp1c[16];      // games played - games won - door open - pwr reset
                         // Contadores de billetes
  uint8_t lp31[4];       // $1 bills
  uint8_t lp32[4];       // $2 bills
  uint8_t lp33[4];       // $5 bills
  uint8_t lp34[4];       // $10 bills
  uint8_t lp35[4];       // $20 bills
  uint8_t lp36[4];       // $50 bills
  uint8_t lp37[4];       // $100 bills
  uint8_t lp3a[4];       // $200 bills
  uint8_t lp38[4];       // $500 bills
  uint8_t lp39[4];       // $1000 bills
  uint8_t lp3c[4];       // $2000 bills
  uint8_t lp3f[4];       // $5000 bills
  uint8_t lp40[4];       // $10000 bills
  uint8_t lp41[4];       // $20000 bills
  uint8_t lp43[4];       // $50000 bills
  uint8_t lp44[4];       // $100000 bills
                         /* Contadores de longitud variable (LP extendido 6F o AF).
                          * 1° byte: longitud del contador (en bytes)
                          * 2° byte en adelante: contador.
                          */
  uint8_t lp6f0c[10];    // current credits
  uint8_t lp6f00[10];    // total coin in
  uint8_t lp6f01[10];    // total coin out
  uint8_t lp6f24[10];    // total drop
  uint8_t lp6f02[10];    // total jackpot
  uint8_t lp6f04[10];    // total cancelled credits
  uint8_t lp6f17[10];    // total EFT to VGM (credits)
  uint8_t lp6f18[10];    // total EFT to host (credits)
  uint8_t lp6f0d[10];    // total cashable tickets in (cents)
  uint8_t lp6f0e[10];    // total cashable tickets out (cents)
  uint8_t lp6f03[10];    // total hand paid cancelled credits
  uint8_t lp6f20[10];    // total attendant paid progressive bonus win (credits)
  uint8_t lp6f21[10];    // total attendant paid external bonus win (credits)
  uint8_t lp6f1d[10];    // total machine paid progressive win (credits)
  uint8_t lp6f1e[10];    // total machine paid external bonus win (credits)
  uint8_t lp6f0b[10];    // total credits from bills accepted

  uint16_t mac[3];         // dirección MAC del módulo
  uint16_t uid;            // UID de la VGM
  uint16_t hash;           // Obs.: se debe setear antes que vgmAddr, para no sobreescribirlo
  uint16_t crc;
} LongPollMsgType;

/*
 * Estructura para los mensajes de Long Poll a pedido del servidor
 */
typedef struct __attribute__((packed))
{
  uint8_t length;     // longitud del comando o respuesta
  union __attribute__((packed))
  {
    uint8_t cmd[128];
    uint8_t rsp[128];
  };
  uint8_t mac[6];     // dirección MAC del módulo
  uint16_t uid;       // UID de la VGM
  uint16_t hash;
  uint16_t crc;
} OnDemandLongPollMsgType;

// longpoll.c  -----------------------------------------------------------------------------

//---------------------------------------------------------------------------------
// Efectúa un long poll y almacena el resultado en el buffer correspondiente.
// Cada vez efectúa un long poll con un código diferente.
// Parámetro: np --> si < 0 realiza el LP que corresponda según la secuencia cíclica
//                   si >= 0 realiza un LP en particular a pedido
// Retorna rx_buflen --> 0 indica que no hubo respuesta.
//---------------------------------------------------------------------------------
uint32_t polling( int np );

// Indices a determinados LP que se pueden solicitar específicamente
#define NPOLL_FEATURES                          0   // Indice al comando del LP A0
#define NPOLL_DENOMINATION                      1   // Indice al comando del LP 1F
#define NPOLL_CURRENT_CREDITS                  16
#define NPOLL_COIN_IN                          17
#define NPOLL_TOTAL_HANDPAID_CANCELLED_CREDITS 19
#define NPOLL_TOTAL_TICKETS_IN_OUT             22

#define NPOLL_DENOMINATION 1  // Indice al comando del LP 1F
#define NPOLL_VGM_ID 1        // Indice al comando del LP 1F

// Parámetro para que polling() haga el próximo LP según la secuencia cíclica
#define NEXT_CICLIC_POLL -1

/*
 * Buffer del mensaje de contadores.
 * Sobre el mismo se van almacenando los valores obtenidos mediante los distintos LP.
 */
extern LongPollMsgType longPollBuffer;

// Prototipos de funciones

uint32_t polling( int np );
void long2str (char *d, uint32_t m, int n);
void long2bcd (uint8_t *d, uint32_t m, int n);
uint32_t bcd2long (uint8_t *d, int n);
void clearLongPollBuffer (void);
int getCurrentCredits (uint32_t *vgm_credits);
void OnDemandLongPoll( const ip_addr_t * servAddr, uint16_t port, struct udp_pcb *upcb, struct pbuf * p );
void sendMeters( void );

#endif /* INC_LONGPOLL_H_ */
