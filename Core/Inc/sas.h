/*
 * sas.h
 *
 *  Created on: Oct 2, 2026
 *      Author: guillermo
 */

#ifndef INC_SAS_H_
#define INC_SAS_H_

// Handlers de dispositivos
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart5;
//extern DMA_HandleTypeDef hdma_uart5_rx;

// Tipos definidos usados para definir variables en sas.c

/*-------------------------------------------------------------------
 * Unión para almacenar los distintos flags usados por el programa.
 *-------------------------------------------------------------------
 */
union FLAGS
{
  uint32_t flags;   // Obs.: sólo comprende los primeros 32 bits
  struct
  {
    uint32_t waitingEventAck          :1; // indica que está esperando el ACK a un mensaje de eventos
    uint32_t waitingCashinReply       :1; // indica que espera respuesta a un mensaje de cashin
    uint32_t waitingCashoutReply      :1; // indica que espera respuesta a un mensaje de cashout
    uint32_t waitingHandpayReply      :1; // indica que espera respuesta del servidor de online, por un pago manual
    uint32_t waitingCashinNakEnd      :1; // En "1" indica que se está esperando un mensaje de CASHIN_NAK_END
    uint32_t waitingBonusCashinReply  :1; // En "1" indica que se está esperando una respuesta de cashin, del servidor de bonus
    uint32_t waitingBonusCashoutReply :1; // En "1" indica que se está esperando una respuesta de cashout, del servidor de bonus
    uint32_t waitingBonusNakEnd       :1; // En "1" indica que se está esperando una respuesta a un NAK, del servidor de bonus
    uint32_t waitingHelloReply        :1; // En "1" indica que se está esperando una respuesta a un Hello enviado al servidor
    uint32_t waitingCardReaderTimer   :1; // indica que se está esperando el vencimiento del timer
    uint32_t waitingClientCard    :1;     // indica que se está esperando que se ingrese una tarjeta de cliente
    uint32_t cardReaderLocked     :1;     // indica bloqueo del lector de tarjetas
    uint32_t cardStatus           :1;     // Último estado de la tarjeta -> 0: ausente - 1: presente
    uint32_t forceCashout         :1;     // indica que se debe forzar un cashout
    uint32_t testMode             :1;     // indica que se está en modo test
    uint32_t rx_flag              :1;     // señaliza que se recibió un byte por la UART0
    uint32_t rx_error             :1;     // señaliza que se recibió un byte erróneo por la UART0
    uint32_t rx_wakeup            :1;     // señaliza que se recibió un byte por la UART0, con el bit de wakeup activado
    uint32_t debug                :1;     // En "1" indica que está habilitado el envío de logs para debug al puerto de test
    uint32_t doorOpen             :1;     // En "1" indica puerta abierta
    uint32_t vgmAlive             :1;     // En "1" indica que la VGM está conectada y responde
    uint32_t cashoutButtonPressed :1;     // Ha sido presionado el botón de pago (puede venir un evento 51)
    uint32_t handpayRequest       :1;     // Hay un pago manual pendiente que debe ser reseteado
    uint32_t enableBolaExtra      :1;     /* En "1" indica que se puede cargar crédito en medio de una jugada,
                                             para lo que al retirar la tarjeta no descarga la máquina sino que
                                             hace un cashout en cero */
    uint32_t fechaHoraValida      :1;     /* En "1" indica que ya recibió una respuesta de hello,
                                             y fijó la fecha enviada por el servidor */
    uint32_t sendCoinIn           :1;     /* En "1" enviar mensaje de contadores al servidor
                                             encargado de las estadísticas de los progresivos */
    uint32_t payLegacyBonus       :1;     // En "1" cargar un Legacy Bonus en la máquina
    uint32_t validationSystemOK   :1;     /* En "1" indica que debe usarse el validation number guardado.
                                             En "0" indica que debe generarse un nuevo validation number
                                             Obs.: esto es para TITO con validation system. */
    uint32_t ticketValidation     :1;     // Enhanced 0 - System 1
    uint32_t forceQuitCard        :1;     // En "1" cuando arranca con la tarjeta puesta, para forzar a quitarla
    uint32_t watchdogInit         :1;     // En "1" indica que tuvo un reinicio por watchdog
    uint32_t lockModule           :1;     // Poner a "1" para dejar el módulo bloqueado en un loop
    uint32_t actualizarFechaHora  :1;     // En "1" indica incrementar en 1" la fecha-hora interna
    uint32_t setVGMDateTime       :1;     // En "1" indica que se debe setear la fecha y hora en la VGM
    uint32_t progressiveWin       :1;     // En "1" indica que se pagó un premio progresivo y se lo debe informar
    uint32_t legacyBonusWin       :1;     // En "1" indica que se pagó un premio legacy bonus y se lo debe informar
    uint32_t lockVGM              :1;     // En "1" indica que se debe mantener bloqueada la VGM
    uint32_t VGMjugando           :1;     /* Se pone en 1 cuando recibe el evento 7E
                                             y vuelve a 0 cuando recibe el evento 7F */
    uint32_t faltaDescargandoJ    :1;     /* En "1" indica que hubo una descarga por pago manual y luego falta la descarga
                                             por retiro de tarjeta. Se usa para no poner el cartel de "MAQUINA LIBRE / INGRESE TARJETA"
                                             cuando todavía falta realizar esa descarga */
    uint32_t cashoutWin           :1;     /* La máquina ordenó descargar un premio (evento 6B) */
    uint32_t md5Error             :1;     /* En "1" avisa que está en el loop de error MD5, para que no modofique el display */
    uint32_t waitingEvent52       :1;     /* Apareció un pago manual (evento 51) y se debe esperar
                                             al evento 52 antes de permitir una descarga en cero. */
    uint32_t extendedLongPolls    :1;     // En "1": La VGM soporta LP extendidos
    uint32_t sendLongPollEnabled  :1;     /* En "1": habilitado el envío de mensajes de contadores al servidor (luego de
                                             haber hecho una vuelta completa de polling. */
    uint32_t denominacion_default :1;     /* En "1": por defecto usa el valor de denominación de un centavo. Es para las
                                             máquinas que no informan la denominación con el LP 1F (por ej.: ruletas Dayco) */
    uint32_t onDemandLongPoll     :1;     // En "1" indica que se recibió un requerimiento de long poll a demanda, que debe ser procesado
    uint32_t updatingFirmware     :1;     // En "1" indica que está en proceso de recibir una nueva imagen del firmware
    uint32_t llamado_al_bar       :1;     /* En "1": hay un llamado al bar pendiente de atención */
  };
};

/*
 * Estructura que contiene las opciones de configuración de la interacción con la máquina.
 * Obs.: hay opciones que no pueden estar habilitadas al mismo tiempo; por ej.: cashless
 * y tito. Si es EIBE, automáticamente implica que es cashless con bola extra y TITO
 * es deshabilitado.
 */
union CONFIG_VGM
{
  uint32_t config;
  struct
  {
    uint32_t eibe :1;        // En "1" indica que la máquina es una EIBE, que
                             // tiene algunas diferencias en cuanto al SAS
    uint32_t cashlessBolaExtra :1; // En "1" indica cashless habilitado para cargar
                                   // crédito en medio de una partida para permitir
                                   // jugar una "bola extra" (bingo)
  };
};

// Variables extern definidas en sas.c
extern volatile union FLAGS flag, flagsWaitingServerReply;

#define waitingAnyServerReply() ((flag.flags & flagsWaitingServerReply.flags)?1:0)

extern volatile union CONFIG_VGM cfg;

/* Estructura para definir los flags que controlan las operaciones de tickets.
   Como esta versión está prevista para operar sólo con cashless, sólo se
   atienden algunos pocos eventos de TITO que -probablemente por mala configuración
   de la máquina- puedan aparecer.
*/
extern uint32_t uid;       // UID de la VGM, según el padrón de la sala

extern uint8_t MyMAC[6];       // Dirección MAC

extern uint32_t nRetry2LockVGM; // Nro de intentos de envío de Hello antes de bloquear la VGM

extern uint32_t currentCredits;       // último valor obtenido con el LP 1A

extern uint8_t vgmAddr;           /* Dirección de la máquina (para EIBE usa 0x01,
                                     para SAS detecta la dirección que tenga) */
extern uint32_t pollingPeriod;    // Periodo entre envío de pollings a la máquina

extern uint32_t montoLegacyBonus; // Monto a cargar por Legacy Bonus

typedef struct
{
  uint32_t do_ticket_out :1; // Evento 3D o 3E --> se imprimió un ticket o se emitió un pago manual
  uint16_t do_tito_handpay_3e:   1;/* La VGM generó un pago manual sin ticket. Se lo trata igual
                                      que a un ticket pero, por ahora, no se lo envía al servidor. */
  uint32_t do_set_validation_id :1;/* Enviar validation number a la VGM para que imprima un ticket
                                      y obtener el monto del mismo para enviarlo al servidor */
} TICKET_FLAGS;

extern volatile TICKET_FLAGS ticket_flag;

/* Variable usada para ordenar una operación de cashless, como consecuencia de una acción
   del jugador: ingresar tarjeta, retirar tarjeta, presionar botón de cashout, etc. */
extern int cash_flag;

// Constantes para identificar la operación de cashless a realizar, en la variable cash_flag
#define DO_CASHIN  1   // Ordena hacer un cashin
#define DO_CASHOUT 2   // Ordena hacer un cashout

extern uint32_t unidad_denominacion; // denominación en centavos según código en LP1F

extern char vgmID[];  // Código de 2 char que identifica al fabricante. Por ej.: "EE" --> EIBE (ver tabla C-1 del manual del SAS)

/* Macros para habilitaciones y deshabilitaciones */
#define CMD_LOCK_VGM              0x01
#define CMD_UNLOCK_VGM            0x02
#define CMD_ENABLE_BILL_ACCEPTOR  0x06
#define CMD_DISABLE_BILL_ACCEPTOR 0x07

#define EnableVGM()  enableDisableLP(CMD_UNLOCK_VGM)
#define DisableVGM() enableDisableLP(CMD_LOCK_VGM)

#define EnableBillAcceptor()  enableDisableLP(CMD_ENABLE_BILL_ACCEPTOR)
#define DisableBillAcceptor() enableDisableLP(CMD_DISABLE_BILL_ACCEPTOR)

// Prototipos de funciones de acceso público definidas en sas.c
int enableDisableLP( uint8_t cmd );
void delay(uint32_t mseg);
void processEventAck (struct pbuf *udp_rx_buf);

#endif /* INC_SAS_H_ */
