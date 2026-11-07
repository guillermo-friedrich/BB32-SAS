/*
 * aft.h
 *
 *  Created on: Oct 10, 2026
 *      Author: guillermo
 */

#ifndef INC_AFT_H_
#define INC_AFT_H_

/* ---------------------------------------------------------------------
 * Estructuras de los mensajes de cashless entre el módulo y el servidor
 * ---------------------------------------------------------------------
 */
// Estructura para los mensajes de cashless entre el módulo y el servidor
typedef struct __attribute__((packed))
{
  struct __attribute__((packed))
  {
    uint8_t msgCode;

    // El 2do byte tiene distinto significado según el momento en que se lo usa
    union __attribute__((packed))
    {
      // En CASHOUT_REQ, el módulo reporta en error code el transfer status de AFT, EFT o DACOM.
      // El servidor usa error code para reportar la causa de rechazo a un pedido
      // En 0 significa que no hay error
      uint8_t errorCode;

      // En CASHIN_REQ el módulo indica la opción requerida para la carga
      // 0 : pedido de carga estándar
      // 1 : si la tarjeta es de cliente --> carga estándar
      //     si la tarjeta es de vendedor --> consulta el crédito disponible del vendedor, para
      //                                      efectuar el ingreso manual desde el touch screen
      // 2 : pedido de carga de un vendedor --> en el monto se indica el monto a cargar,
      //                                        ingresado manualmente por el vendedor
      uint8_t cashinOption;
    };

    uint8_t protocol;   // Usado en CASHIN_REP_CLI_OK o CASHIN_REP_VEND_OK,
                        // para que el servidor indique el protocolo cashless
                        // a usar: 'E': EFT, 'A': AFT, 'D': DACOM, 'T': TITO
  };

// El 4to campo tiene distinto significado según el momento en que se lo usa
  union __attribute__((packed))
  {
    struct __attribute__((packed))
    {
      uint8_t cardID[32];     // 31 ASCII ID de la tarjeta + NULL
                              // En los mensajes de CASHIN_... va el ID de la tarjeta
      union __attribute__((packed))
      {
        char pin[5];         // PIN ingresado por el usuario, ASCII + NULL
                             // Se envía en el mensaje de CASHIN_REQ
                             // -------------------------------------------
        char saldo[11];      // Saldo del cliente (10 dígitos + NULL).
                             // Lo envía el servidor en el CASHIN_REP_CLI_OK y en el
                             // CASHIN_REP_VEND_OK (para los módulos con teclado)
                             //--------------------------------------------
        char montoPesos[11]; // Monto en pesos (para montar en el display) de una carga
                             // de vendedor. Viene en el CASHIN_REP_VEND_OK (para los
                             // módulos sin teclado, que cargan por múltiplos del valor
                             // asignado a cada tarjeta).
        struct __attribute__((packed))
        {
          uint8_t unused[5]; // Para saltear los bytes usados por pin[]
          uint8_t drop[4]; // Valor del contador de drop obtenido antes de la carga.
                           // Se usa en el mensaje CASHIN_REQ
        };
      };
      // contadores extraidos de los LP 1C y 2F, cuyos valores
      // han sido obtenidos inmediatamente antes de la carga y la descarga
      uint8_t coin_in[4];
      uint8_t coin_out[4];
      uint8_t jackpot[4];
      uint8_t total_handpay_cancelled_credits[4];
      uint8_t electronic_transfers_in[4];
      uint8_t electronic_transfers_out[4];
    } ID;

    struct __attribute__((packed))
    {
      uint8_t date[4];              // BCD - MMDDYYYY
      uint8_t time[3];              // BCD - HHMMSS
      uint8_t validation_number[9]; // BCD -
      uint8_t ticket_number[2];     // binario 0000 a 9999
      uint8_t expiration[4];       // BCD - MMDDYYYY o número de días (1 a 9999)
    } ticket;

    struct __attribute__((packed))
    {
      uint8_t lp1c[28]; // coin in - coin out - drop - jackpot - games played - games won - door open (pwr reset no va)
                        // En los mensajes de CASHOUT_..._REQ van los valores del LP1C,
                        // que el servidor usa para estadísticas de juego.

      uint8_t total_credits_from_bills_accepted_cashout[4]; // Últimos 4 bytes del LP2F (en lp2fb)

      // También va la fecha y hora, para saber (en caso de corte) a que
      // hora se produjo la descarga o pago manual
      union __attribute__((packed))
      {
        struct __attribute__((packed))
        {
          char anio[4];    // Año: 4 dígitos ASCII
          char mes[2];     // Mes: 2 dígitos ASCII ("01" al "12")
          char dia[2];     // Día: 2 dígitos ASCII ("01" al "31")
          char hora[2];    // Hora: 2 dígitos ASCII ("00" al "23")
          char min[2];     // Minutos: 2 dígitos ASCII ("00" al "59")
          char seg[2];     // Segundos: 2 dígitos ASCII ("00" al "59")
        };
        char datetime[15];  // Array que abarca todos los campos de fecha y hora
      };

      union __attribute__((packed))
      {
        /* total electronic transfers to VGM - idem to host - total hand paid cancelled credits
         * En los mensajes de CASHOUT_..._REQ van los valores del LP2F, de donde
         * el servidor obtiene total hand paid cancelled credits para estadísticas de juego.
         */
        uint8_t lp2f[12];
        uint8_t lp10[4];  // Jurisdictional cancel credit meter.
                          // Se usa en el mensaje de pago manual
      };

      /* Para los mensajes que se transmiten al puerto de handpay
       * del servidor online (1207), este campo permite distinguir
       * si se trata de un pago manual (51), un premio del
       * progresivo sin pago manual (54) o un legacy bonus (7C) */
      uint8_t evt;

      union __attribute__((packed))
      {
        // Variables usadas cuando se reporta un premio por progresivo
        struct __attribute__((packed))
        {
          /* En caso de que un pago manual provenga de un progresivo,
           *  este campo indica el grupo al que pertenece. Si no, va en cero */
          uint8_t progGroup;

          /* En caso de que un pago manual provenga de un progresivo,
           * este campo indica el nivel del pozo sobre el que se pagó el premio */
          uint8_t progLevel;
        };

        // Variables usadas cuando se reporta un premio por legacy bonus
        struct __attribute__((packed))
        {
          uint8_t multiplier;
          uint8_t tax_status;
        };
      };
    };

    /* 4 renglones de 16 ASCII + null.
     * El servidor lo envía en la respuesta de cashout,
     * para que el módulo lo presente en el display */
    char msg[4][17];
  };
  union __attribute__((packed))
  {
    // En ambos casos son 10 dígitos en ASCII + NULL
    char monto[11]; // Lo usa para indicar el monto a cargar a la VGM (CASHIN_REP_xx_OK)
                    // o el monto descargado de la VGM (CASHOUT_REQ) */

    char saldoFinal[11]; // Lo usa en el CASHOUT_REP_OK para indicar el saldo final
                         // en la cuenta del cliente.

    struct __attribute__((packed))
    {
      char monto_vendedor[7]; // Usado en el cashin para indicar el monto cargado por el vendedor

      uint8_t total_credits_from_bills_accepted_cashin[4]; // Últimos 4 bytes del LP2F (en lp2fb)
    };
  };
  uint16_t mac[3];  // dirección MAC del módulo
  uint16_t uid;     // UID de la VGM
  uint16_t hash;
  uint16_t crc;
} CashMsgType;

// Códigos para los mensajes UDP de transacciones cashless (formato CashMsgType)

// msgCode:
#define CASHIN_REQ            0 // MOD->SERV - inicia un pedido de cash - envía el ID de la tarjeta
#define CASHIN_REP_CLI_OK     1 // SERV->MOD - respuesta afirmativa a un pedido de cash de un cliente
#define CASHIN_REP_VEND_OK    2 // SERV->MOD - respuesta afirmativa a un pedido de cash de un vendedor
#define CASHIN_REP_VEND_MAX  22 // SERV->MOD - el servidor informa el máximo que puede cargar el vendedor
#define CASHIN_REP_REJ        3 // SERV->MOD - respuesta negativa a un pedido de cash
#define CASHIN_ACK            4 // MOD->SERV - confirmación de la transacción
#define CASHIN_NAK            5 // MOD->SERV - rechazo de la transacción
#define CASHIN_NAK_END        6 // SERV->MOD - el servidor confirma que recibió el rechazo de la transacción

#define CASHOUT_REQ       10    // MOD->SERV - inicia una descarga de cash - envía el monto
#define CASHOUT_REP_OK    11    // SERV->MOD - respuesta afirmativa a una descarga de cash
#define CASHOUT_REP_REJ   12    // SERV->MOD - respuesta negativa a una descarga de cash
#define CASHOUT_ACK       13    // MOD->SERV - confirmación de la transacción
#define CASHOUT_NAK       14    // MOD->SERV - rechazo de la transacción

#define CASHOUT_HANDPAY_REQ 20  /* MOD->SERV - inicia una descarga de cash originada por un
                                               pago manual. Envía el monto en créditos */

#define CASHIN_TICKET_REQ   30  /* MOD->SERV - envía al servidor los datos de un ticket ingresado,
                                   para que el servidor lo valide y envíe el monto
                                   a cargar a la VGM. */
#define CASHOUT_TICKET_REQ  40  // MOD->SERV - envía al servidor el monto y los datos de un pago con ticket

#define LEGACY_BONUS_VERSION_1

#ifdef LEGACY_BONUS_VERSION_1
#define LEGACY_BONUS_REQ    50  // SERV->MOD - envía el monto de legacy bonus a cargar en la máquina
#define LEGACY_BONUS_ACK    51  // MOD->SERV - Legacy Bonus cargado OK; error_code va en cero
#define LEGACY_BONUS_NAK    52  // MOD->SERV - Falló carga del Legacy Bonus. En error_code va el código de error
#else
#ifdef LEGACY_BONUS_VERSION_2
#define LEGACY_BONUS_INIT        50  // SERV->MOD - iniciar una operación de legacy bonus
#define LEGACY_BONUS_REQ         51  // MOD->SERV - solicita el monto de legacy bonus a cargar en la máquina
#define LEGACY_BONUS_REPLY       52  // SERV->MOD - envía el monto de legacy bonus a cargar en la máquina
#define LEGACY_BONUS_ACK         53  // MOD->SERV - Legacy Bonus cargado OK; error_code va en cero
#define LEGACY_BONUS_NAK         54  // MOD->SERV - Falló carga del Legacy Bonus. En error_code va el código de error
#define LEGACY_BONUS_NAK_END     55  // SERV->MOD - El servidor confirma la recepcin del NAK

#define LEGACY_BONUS_CASHOUT_REQ 56      // MOD->SERV - el módulo envía el monto descargado al servidor de bonus
#define LEGACY_BONUS_CASHOUT_REP_OK  57  // SERV->MOD - el servidor de bonus acepta la descarga recibida
#define LEGACY_BONUS_CASHOUT_REP_REJ 58  // SERV->MOD - el servidor de bonus rechaza la descarga recibida
#else
#error "Versión de Legacy Bonus no definida"
#endif // LEGACY_BONUS_VERSION_2
#endif // LEGACY_BONUS_VERSION_1

#define FIDELITY_IN         60  // MOD->SERV - Envía datos de la tarjeta de fidelity ingresada y contadores
#define FIDELITY_OUT        61  // MOD->SERV - Envía datos de la tarjeta de fidelity retirada y contadores
#define FIDELITY_ACK        62  // SERV->MOD - Tarjeta de fidelity aceptada
#define FIDELITY_NAK        63  // SERV->MOD - Tarjeta de fidelity rechazada (en errorCode se informa la causa)

// errorCode: códigos de error reportados por el servidor luego de un pedido de CASHIN o CASHOUT
#define CASH_REQ_OK                  0
#define CASH_TARJETA_INVALIDA        1
#define CASH_PIN_INVALIDO            2
#define CASH_REINGRESAR_PIN          3
#define CASH_TARJETA_BLOQUEADA       4
#define CASH_VENDEDOR_SIN_FONDOS     5
#define CASH_ASOCIADA_A_OTRO_SLOT    6
#define CASH_NO_HAY_TARJETA_ASOCIADA 7  // Cuando en una descarga la VGM no tiene una tarjeta asociada
#define CASH_MONTO_INVALIDO          8
#define CASH_MONTO_EXCESIVO          9  // Monto ingresado manualmente por el vendedor
#define CASH_FALLO_DESCARGA         10  // El servidor no pudo procesar la descarga (viene con un CASHOUT_REP_REJ)
#define CASH_CARGA_DIARIA_EXCEDIDA  11  // Superó el monto diario máximo (esto es para evitar el lavado de dinero)
#define CASH_MAQUINA_BLOQUEADA      12  //
#define CASH_MAQUINA_NO_PERMITIDA   13  // No permitida la carga de vendedor en esta máquina

// cashinOption, cuando se envía un mensaje CASHIN_REQ

#define CASHIN_STD    0   // Módulo básico --> procedimiento de carga estándar

#define CASHIN_TOUCH  1   /* La VGM tiene PC con touch para que el vendedor
                             ingrese manualmente el monto a cargar. Si la tarjeta es de un
                             vendedor, el servidor responde con CASHIN_REP_VEND_MAX y el monto
                             máximo que puede cargar.
                             Si la tarjeta es de un cliente, el servidor responde con
                             CASHIN_REP_CLI_OK. */

#define CASHIN_VEND   2   /* El vendedor ingresó manualmente el monto a cargar, y va indicado en
                             el campo "monto". */

// Variables públicas
extern uint8_t cashProtocol;
extern CashMsgType cashMsgBuffer;   // Buffer para los mensajes de cashless a enviar
extern uint32_t montoVendedor;      /* Totaliza la última carga de un vendedor (en centavos o créditos, según la VGM).
                                       Se usa para cargar o descargar */

// Prototipos de funciones
// aft.c ---------------------------------------------------------------------------------

uint8_t AFT_getTransactionID (uint16_t interrogate);

void prepareCashMsgBuffer(void);

void prepareHandpayMsgBuffer(void);

int filtrarEvento (uint8_t *evtPtr);

int getEvento (uint8_t *evt);

void sendEvents(void);

void delay80 (uint32_t delayTime);

void sendCashinAckNak (uint8_t errorCode);

#define sendCashinAck()  sendCashinAckNak(0)
#define sendCashinNak(e) sendCashinAckNak(e)

void processCashlessReply (struct pbuf *udp_buf);

void cashin(void);
void cashout(void);
uint8_t LegacyBonus(void);
uint8_t reset_handpay( uint8_t evt );



#endif /* INC_AFT_H_ */
