/*
 * mgmt.h
 *
 *  Created on: 25 nov. 2019
 *      Author: guillermo
 */

#ifndef INC_MGMT_H_
#define INC_MGMT_H_

/* Constantes y macros ------------------------------------------------*/
/* Máscaras de bits */
#define BIT0 0x01
#define BIT1 0x02
#define BIT2 0x04
#define BIT3 0x08
#define BIT4 0x10
#define BIT5 0x20
#define BIT6 0x40
#define BIT7 0x80

/* Defines relacionados con la variable Features, de tipo FeaturesType */
#define LCD_2x16     0
#define LCD_128x64   1

#define TECLADO_NO   0

#define UID_NO       0

#define FONT_DEFAULT 0

#define CASHLESS_ENABLED                BIT0  // habilita cashless (si no, puede ser TITO o sólo online)
#define CASHLESS_ENABLED_BOLA_EXTRA     BIT1  // acepta carga con la máquina cargada
#define DROP_LP1D                       BIT2  // obtiene drop, EFTin y EFTout mediante LP1D
#define NO_LOCK_AFT_CASHOUT             BIT3  // descarga AFT sin bloquear
#define AFT_CASHOUT_LP1A                BIT4  // descarga AFT sin bloquear y consultando saldo con LP1A (ruletas electrochance)

#define TITO_DISABLED  0
#define TITO_ENABLED   1

// constantes usadas para el código de operación de los mensajes de administración

#define MGMT_GET_VERSION       1  // SRV -> MOD: Consultar la versión actual en el módulo
#define MGMT_SEND_VERSION      2  // MOD -> SRV: Módulo informa versión actual
#define MGMT_SET_VERSION       3  // SRV -> MOD: Ordenar la instalación de una cierta versión
#define MGMT_SET_IP_MAC        4  // SRV -> MOD: Ordena setear IP y MAC
#define MGMT_GET_CODESEG       5  // MOD -> SRV: módulo pide un cierto segmento de una nueva versión
#define MGMT_SEND_CODESEG      6  // SRV -> MOD: Servidor envía un segmento de código de una nueva versión
#define MGMT_ABORT_DWNLD       7  // MOD -> SRV: módulo informa que abortó descarga de una nueva versión
#define MGMT_SEND_BITMAP       8  // SRV -> MOD: Servidor envía una porción de un bitmap
#define MGMT_GET_FEATURES      9  // SRV -> MOD: Consultar las features configuradas en el módulo
#define MGMT_SEND_FEATURES    10  // MOD -> SRV: Módulo informa las features configuradas
#define MGMT_SET_FEATURES     11  // SRV -> MOD: Servidor ordena configurar features del módulo
#define MGMT_DELE_FEATURES    12  // SRV -> MOD: Servidor ordena borrar features (vuelve a valores por defecto)
#define MGMT_GET_HASH_MD5     13  // SRV -> MOD: Servidor pide el hash MD5 del código (0x1100 a 0xFFFF)
#define MGMT_SEND_HASH_MD5    14  // MOD -> SRV: Módulo envía el hash MD5 del código
#define MGMT_GET_HASH_CRC16   15  // SRV -> MOD: Servidor pide el hash CRC16 del código (0x1100 a 0xFFFF)
#define MGMT_SEND_HASH_CRC16  16  // MOD -> SRV: Módulo envía el hash CRC16 del código
#define MGMT_CLEAR_MD5        17  // SRV -> MOD: Servidor ordena borrar el MD5 grabado en EEPROM para autoverificación
#define MGMT_ERASE_EEPROM     18  // SRV -> MOD: Servidor ordena borrar toda la EEPROM
#define MGMT_RESET            19  // SRV -> MOD: Servidor ordena resetear el micro
#define MGMT_CHANGE_NETWORK   20  // SRV -> MOD: Servidor ordena cambiar de red IP
#define MGMT_SET_IP_FEATURES  21  // SRV -> MOD: Servidor ordena cambiar IP y configurar features del módulo
#define MGMT_DUMP_MEMORY      22  /* SRV -> MOD: Servidor pide al módulo que le envíe el contenido de un bloque de memoria
                                     MOD -> SRV: Módulo envía el contenido de un bloque de memoria */
#define MGMT_SET_TICKET_DATA  23  // SRV -> MOD: Servidor envía los datos fijos del ticket
#define MGMT_SEND_TICKET_DATA 24  // MOD -> SRV: Módulo envía al servidor los datos fijos del ticket que tiene grabados

#define MGMT_CRC_SEED 0x968B   /* valor inicial para el cálculo del CRC (usado como password).
                                  Las versiones anteriores a la 6.00 usaban 0x5AC3. Se cambió
                                  para que los servidores y cashtest antiguos sean incompatibles
                                  con el firmware de versiones 6.00 y superiores. */

/* Typedefs -----------------------------------------------------------*/

/*-------------------------------------------------------------------------------
* Estructura para almacenar datos fijos de los tickets
* Obs.: no usada para cashless, pero se deja por compatibilidad del formato
*       de la estructura Features
*--------------------------------------------------------------------------------
*/
typedef struct  __attribute__((packed))
{
  uint16_t  expiration; // cantidad de días de validez del ticket
  char location[30];    // nombre de la sala
  char address1[24];    // dirección -1ra línea-
  char address2[24];    // dirección -2da línea-
} TicketDataType;

/*----------------------------------------------------------------------
 * Estructura para almacenar los parámetros de configuración del módulo
 *----------------------------------------------------------------------
 */
typedef struct __attribute__((packed))
{
  uint16_t display;    /* 00 o FF : display de texto 2x16
                          01 : display gráfico monocromo de 128 x 64 pixels (ks0108)
                          02 : display en módulo conectado por IP */
  uint16_t teclado;    /* 00 o FF : sin teclado
                          01 : teclado o touch screen en módulo conectado por UART
                          02 : teclado o touch screen en módulo conectado por IP */
  uint16_t uid;        // 00 o FF : no tiene UID seteado

  uint16_t font;

  uint16_t tito;        /* 01: TITO habilitado
                               Otro valor: TITO deshabilitado */

  uint16_t cashless;    /* 00: cashless deshabilitado (sólo online)
                           01: cashless habilitado
                           02: cashless deshabilitado (sólo online) - Ruletas Golden
                           03: cashless habilitado - permite carga de créditos en medio del juego, para bola extra */

  uint32_t IP_server_online; // IP del servidor de online (eventos, contadores, pagos manuales)
  uint32_t IP_server_cash;   // IP del servidor de cashless
  uint32_t IP_server_mgmt;   // IP del servidor de administración
  uint32_t IP_server_prog;   // IP del servidor de progresivo
  uint32_t IP_server_extra;  // IP reservada para un futuro servidor (sin uso actualmente)

  TicketDataType ticketData;   // Datos fijos a imprimir en los tickets
} FeaturesType;

//-------------------------------------------------------------------------------
// Estructura para los mensajes de gestión
//-------------------------------------------------------------------------------

typedef union __attribute__((packed))
{
  struct __attribute__((packed))
  {
    uint16_t op;          // código de la operación
    char     version[20]; // Nro de versión en ASCII
    uint8_t  IP[4];       // dirección IP (ej.: 172.16.0.20, en ese orden)
    uint16_t offset;      // offset inicial de un bloque de 512 bytes (LSB-MSB)
    uint16_t crc;         // CRC generado a partir de un valor inicial
  } msg1;

  struct __attribute__((packed))
  {
  	uint16_t op;          // código de la operación
    char version[20];     // Nro de versión en ASCII
    uint16_t offset;      // offset inicial del bloque de código o datos de 512 bytes
    union __attribute__((packed))
    {
    	uint16_t code[256]; // segmento de código de 512 bytes
    	uint8_t  data[512]; // bloque de datos de 512 bytes (por ej: la mitad de un bitmap de 128x64)
    };
    uint16_t crc;         // CRC generado a partir de un valor inicial
  } msg2;

  struct __attribute__((packed))
  {
  	uint16_t op;              // código de la operación
    FeaturesType features;    // características opcionales del módulo, configurables por software
    uint16_t crc;             // CRC generado a partir de un valor inicial
  } msg3;

  struct __attribute__((packed))
  {
  	uint16_t op;             // código de la operación
    union {
      struct __attribute__((packed))
      {
        int16_t  seedLen;
        uint16_t seed[16];  // semilla inicial para dispersar el cálculo del hash
      };
      uint8_t digest[16];   // resumen MD5
    };
    uint16_t crc;            // CRC generado a partir de un valor inicial
  } msg4;

  struct __attribute__((packed))
  {
  	uint16_t op;           // código de la operación
  	uint8_t IP[4];         // dirección IP (ej.: 172.16.0.20, en ese orden)
    FeaturesType features; // características opcionales del módulo, configurables por software
    uint16_t crc;          // CRC generado a partir de un valor inicial
  } msg5;

  struct __attribute__((packed))
  {
  	uint16_t op;          // código de la operación
  	uint16_t offset;      // offset inicial del bloque de código o datos de 128 bytes
  	uint8_t data[128];    // bloque de 128 bytes de datos / código
  	uint16_t crc;         // CRC generado a partir de un valor inicial
  } msg6;

  struct __attribute__((packed))
  {
  	uint16_t op;               // código de la operación
    TicketDataType ticketData; // datos fijos para los tickets (expiración, nombre de la sala y dirección)
    uint16_t crc;              // CRC generado a partir de un valor inicial
  } msg7;

} MgmtMsgType;

/*
 * Estructura con la que se almacenan los parámetros de configuración en la flash.
 */
typedef struct __attribute__((packed))
{
  uint32_t     flag_uso;    /* ffffffff : libre (no hay datos grabados).
                               cuando se le graba info se lo pone a 0. */
  FeaturesType features;    // 112 bytes
  uint32_t     IP_propia;
  uint32_t     reservado32; // relleno para completar 128 bytes
  uint16_t     reservado16; // relleno para completar 128 bytes
  uint16_t     crc;         // CRC16
}FlashConfigType;

/* Variables externas -------------------------------------------------*/
extern FeaturesType features;
extern uint32_t myCfgIP;
/* Prototipos de funciones --------------------------------------------*/

uint32_t MyIP(void);   // Retorna la dirección IP propia

uint32_t Vcc(void);    // Retorna la tensión de alimentación del micro

void startWatchdog (uint32_t t);
void display_low_vcc (void);
void display_vcc (void);
void setUID(uint16_t uid);           // Setea un nuevo valor de UID
void setIpMistery(uint8_t newIP[]);  // Graba una nueva dirección IP del servidor de Mistery

//void UDP_Mgmt_Init(void);
void udp_mgmt_server_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

#endif /* INC_MGMT_H_ */
