/*
 * aft.c
 *
 *  Created on: Oct 7, 2026
 *      Author: guillermo
 */

#if 0

#include "stdint.h"
#include "cashless.h"

CashMsgType cashMsgBuffer;

uint8_t AFT_getTransactionID (uint16_t interrogate)
{
  return 0;
}

uint8_t LegacyBonus()
{
  return 0;
}

#endif

#include "cashless.h"

static CashMsgType cashRxMsg;  // Buffer para los mensajes de cashless recibidos

CashMsgType cashMsgBuffer;     // Buffer para los mensajes de cashless a enviar

static uint32_t monto = 0;            // Monto a cargar a la VGM

uint32_t montoVendedor = 0;    // Totaliza la última carga de un vendedor (en centavos
                               // o créditos, según la VGM). Se usa para cargar o descargar
uint8_t cashProtocol = 0;

static uint32_t cargaUnitVendedor = 0;  /* Monto unitario, en pesos, correspondiente a cada carga efectuada por un vendedor
                                           (se usa para computar el monto a mostrar en el display
                                           durante una carga de vendedor). */
static uint32_t montoVendedorPesos = 0; // ídem montoVendedor pero en pesos (sólo se usa para mostrar en el display)

static uint8_t AFT_asset_number[4];   // Asset number (se debe configurar igual en todas las VGM)

static uint8_t AFT_transactionID = '1';   // Nro de transacción AFT ('1' a '9')

static volatile uint32_t wait69Timer;

#define wait6FTimer wait69Timer

// Prototipos de funciones static
static uint16_t AFT_lock_status_request (uint8_t cmd, uint8_t transfer_condition, uint32_t *pMonto);
static uint16_t valid_AFT_response (uint8_t cmd);
static void AFT_interrogate (void);
static void AFT_prepare_init (uint8_t transferCode, uint8_t transferType, uint8_t flags, uint32_t monto);
static uint8_t AFT_cashin (uint32_t monto);
static uint8_t AFT_cashout (uint32_t *pMonto);
static uint8_t AFT_cashout_win (uint32_t *pMonto);

/* ------------------------------------------------------------
 * Constantes usadas en distintos campos de los mensajes AFT
 * ------------------------------------------------------------
 */

// transfer code
#define AFT_FULL_TRANSFER_REQ    0x00
#define AFT_PARTIAL_TRANSFER_REQ 0x01
#define AFT_CANCEL_TRANSFER_REQ  0x80
#define AFT_COMPLETE_TRANSFER    0xFF
// transfer type
#define AFT_CASHIN  0x00
#define AFT_CASHOUT 0x80
#define AFT_CASHOUT_WIN 0x90
// transfer flags
#define AFT_HOST_CASHOUT_ENABLE_CTL   0x01
#define AFT_HOST_CASHOUT_ENABLE       0x02
#define AFT_HOST_CASHOUT_MODE_HARD    0x04
#define AFT_CASHOUT_FROM_VGM_REQUEST  0x08
// Transfer status --------------------------------
// Los 3 MSB indican grupos de códigos
#define AFT_TRANSFER_STATUS_MASK          0xE0  // Máscara para ver sólo los 3 MSB
// 000x xxxx OK
#define AFT_TRANSFER_OK                   0x00  // Representa a todo el grupo 000x xxxx
#define AFT_FULL_TRANSFER_OK              0x00
#define AFT_PARTIAL_TRANSFER_OK           0x01
// 010x xxxx pendiente
#define AFT_TRANSFER_PENDING              0x40  // Único código del grupo 010x xxxx
// 100x xxxx transferencia fallida
#define AFT_TRANSFER_FAILED               0x80  // Representa a todo el grupo 100x xxxx
#define AFT_TRANSFER_CANCELLED_BY_HOST    0x80
#define AFT_TRANSACTION_ID_NOT_UNIQUE     0x81
#define AFT_INVALID_TRANSFER_FUNCTION     0x82
#define AFT_INVALID_TRANSFER_AMOUNT       0x83
#define AFT_TRANSFER_AMOUNT_EXCEED_LIMIT  0x84
#define AFT_TRANSFER_AMOUNT_NOT_EVEN_MULTIPLE 0x85
#define AFT_UNABLE_TO_PERFORM_TRANSFER    0x87
#define AFT_ASSET_NUMBER_MISMATCH         0x93
#define AFT_INVALID_TRANSACTION_ID        0x95
#define AFT_UNEXPECTED_ERROR              0x9F
// 110x xxxx transacción incompatible o no soportada
#define AFT_NOT_COMPATIBLE_WITH_IN_PROGRESS 0xC0
#define AFT_UNSSUPORTED_TRANSFER_CODE       0xC1
// 111x xxxx no hay información disponible
#define AFT_NO_TRANSFER_INFO_AVAILABLE    0xFF
// El siguiente no es un código de estado AFT, pero se lo define para
// reportar que no se recibe respuesta a un comando AFT
#define AFT_NO_RESPONSE                   0xE1  /* Clasificado como "no hay info disponible"
                                                   Idem para reportar una respuesta errónea a un comando AFT */
#define AFT_BAD_RESPONSE                  0xE2  /* Clasificado como "no hay info disponible"
                                                   Idem para reportar error porque no se recibió respuesta del servidor */
#define AFT_SERVER_NO_RESPONSE            0xE3  /* Clasificado como "no hay info disponible"
                                                   Idem para reportar error en AFT Lock */
#define AFT_LOCK_ERROR                    0x8F  // Clasificado como "transferencia fallida"

// Eventos AFT
#define AFT_EVENT_TRANSFER_COMPLETE       0x69

// Union que describe los distintos mensajes AFT 72

typedef union __attribute__((packed))
{
  // Estructura del mensaje de AFT (LP 72) usado para subir o bajar centavos
  struct __attribute__((packed))
  {
    uint8_t vgmAddr;                // 0x01
    uint8_t commandCode;            // 0x72
    uint8_t length;                 // 0x35 (53)
    uint8_t transferCode;           // 0x00 Full transfer request  (0x80 cancel transfer request)
    uint8_t transactionIndex;       // 0x00
    uint8_t transferType;           // 0x00 para subir centavos a la máquina, 0x80 para bajar
    uint8_t cashableAmount[5];      // 5 bytes (10 dígitos BCD), cantidad de centavos a transferir
    uint8_t restrictedAmount[5];    // 5 bytes (10 dígitos BCD), todo en cero
    uint8_t nonRestrictedAmount[5];  // 5 bytes (10 dígitos BCD), todo en cero
    uint8_t transferFlags;          // 0x06
    uint8_t assetNumber[4];         // 0x00001234 o 1234 (ver)
    uint8_t registrationKey[20];    // todos ceros
    uint8_t transactionIDLength;    // 0x07
    uint8_t transactionIDtxt[6];    // parte fija del transaction ID "binsur"
    uint8_t transactionID;          // valores variables entre '1' y '9'
    uint8_t expiration[4];          // todos ceros
    uint8_t poolID[2];              // ceros
    uint8_t receiptDataLength;      // 0x00
    uint8_t crc[2];
  } init;

  // Estructura del mensaje AFT para interrogar y/o finalizar una transacción
  struct __attribute__((packed))
  {
    uint8_t vgmAddr;                // 0x01
    uint8_t commandCode;            // 0x72
    uint8_t length;                 // 2
    uint8_t transferCode;           // 0xFF
    uint8_t transactionIndex;       // 0x00 (la última transacción)
    uint8_t crc[2];
  } interrogate;

  // Estructura de la respuesta AFT
  struct __attribute__((packed))
  {
    uint8_t vgmAddr;                // 0x01
    uint8_t commandCode;            // 0x72
    uint8_t length;                 //
    uint8_t transactionBufferPosition;  // 0x00 actual o más reciente
    uint8_t transferStatus;         //
    uint8_t receiptStatus;          //
    uint8_t transferType;           // 0x00 para subir centavos a la máquina, 0x80 para bajar
    uint8_t cashableAmount[5];      // 5 bytes (10 dígitos BCD), cantidad de centavos a transferir
    uint8_t restrictedAmount[5];    // 5 bytes (10 dígitos BCD), todo en cero
    uint8_t nonRestrictedAmount[5];  // 5 bytes (10 dígitos BCD), todo en cero
    uint8_t transferFlags;          // 0x06
    uint8_t assetNumber[4];         // 0x00001234 o 1234 (ver)
    uint8_t transactionIDLength;    // 0x07
    uint8_t transactionIDtxt[6];    // parte fija del transaction ID "binsur"
    uint8_t transactionID;          // valores variables entre 0x01 y 0xFF
    uint8_t transactionDate[4];     // 8 dígitos BCD: MMDDAAAA
    uint8_t transactionTime[3];     // 6 dígitos BCD: hhmmss en formato 24 hs
    uint8_t expiration[4];          // todos ceros
    uint8_t poolID[2];              // ceros
    uint8_t stuff[32];  // relleno --> ojo: ver
  } resp;
} AFT72Type;

// Opciones de comando para LP 74 (AFT status y lock request)
#define AFT_LOCK_REQUEST   0x00
#define AFT_LOCK_CANCEL    0x80
#define AFT_STATUS_REQUEST 0xff
// Opciones de transfer condition para LP 74
#define AFT_LOCK_4_CASHIN  0x01
#define AFT_LOCK_4_CASHOUT 0x02
// Opciones de estado de bloqueo para la respuesta al LP 74
#define AFT_LOCKED         0x00
#define AFT_LOCK_PENDING   0x40
#define AFT_NOT_LOCKED     0xff

typedef union __attribute__((packed))
{
  // Estructura del mensaje de AFT (LP 74) usado para bloquear y consultar estado de AFT
  struct __attribute__((packed))
  {
    uint8_t vgmAddr;                // 0x01
    uint8_t commandCode;            // 0x74
    uint8_t lockCode;               // 0xff (interrogate current status)
    uint8_t transferCondition;      // 0x00 (no importa)
    uint8_t lockTimeout[2];         // todos ceros (no importa)
    uint8_t crc[2];
  } req;

  // Estructura de la respuesta de estado de AFT
  struct __attribute__((packed))
  {
    uint8_t vgmAddr;                // 0x01
    uint8_t commandCode;            // 0x74
    uint8_t length;                 // 23
    uint8_t assetNumber[4];         // 0x00001234 o 1234 (ver)
    uint8_t gameLockStatus;         // 0xff
    uint8_t availableTransfers;     //
    uint8_t hostCashoutStatus;      //
    uint8_t aftStatus;              //
    uint8_t maxBufferIndex;         // 0x46 - 0x7f
    uint8_t cashableAmount[5];      // 5 bytes (10 dígitos BCD), en centavos
    uint8_t restrictedAmount[5];    // 5 bytes (10 dígitos BCD), en centavos
    uint8_t nonRestrictedAmount[5]; // 5 bytes (10 dígitos BCD), en centavos
    uint8_t transferLimit[5];       // 5 bytes (10 dígitos BCD), en centavos
    uint8_t restrictedExpiration[4];
    uint8_t restrictedPoolID[2];
    uint8_t crc[2];
  } resp;
} AFT74Type;

typedef union __attribute__((packed))
{
  // Estructura del mensaje de AFT (LP 73) usado para registrar / desregistrar la VGM
  struct __attribute__((packed))
  {
    uint8_t vgmAddr;                // 0x01
    uint8_t commandCode;            // 0x73
    uint8_t length;                 // 1
    uint8_t registrationCode;       // 0x80 (unregister)
    uint8_t crc[2];
  } req;

  // Estructura de la respuesta de registración AFT
  struct __attribute__((packed))
  {
    uint8_t vgmAddr;                // 0x01
    uint8_t commandCode;            // 0x74
    uint8_t length;                 //
    uint8_t registrationStatus;     // 0x80
    uint8_t assetNumber[4];         // 0x00001234 o 1234 (ver)
    uint8_t registrationKey[20];    //
    uint8_t posID[4];               //
    uint8_t crc[2];
  } resp;
} AFT73Type;

// Estructura del mensaje usado para obtener información de un pago manual pendiente

typedef struct __attribute__((packed))
{
  uint8_t vgmAddr;
  uint8_t command;         // 0x1B
  uint8_t progGroup;
  uint8_t progLevel;
  uint8_t amount[5];
  uint8_t partialPay[2];
  uint8_t resetID;
  uint8_t reserved[10];
  uint8_t crc[2];
} HandpayInfoType;

// Estructura del mensaje de respuesta a un reset de pago manual

typedef struct __attribute__((packed))
{
  uint8_t vgmAddr;
  uint8_t command;         // 0x1B
  uint8_t resetCode;
  uint8_t crc[2];
} ResetHandpayResponseType;

//--- Formato de LP usado para Legacy Bonusing

// Initiate Legacy Bonus (LP 8A)
// La VGM puede responder con ACK (vgmAddr), NAK (vgmAddr | 0x80) o Buys (vgmAddr + 0x00)

typedef struct __attribute__((packed))
{
  uint8_t   vgmAddr;
  uint8_t   command;         // 0x8A
  uint8_t   amount[4];       // monto del bonus, en créditos (8 BCD)
  uint8_t   tax_status;      // 0x00: deducible - 0x01: no deducible - 0x02: wager watch
  uint16_t  crc;
} InitLegacyBonusCommandType;

// Códigos de error para las operaciones de intento de cargar un Legacy Bonus
#define LEGACY_BONUS_OK       0
#define LEGACY_BONUS_REJECT   1
#define LEGACY_BONUS_VGM_BUSY 2
// Los siguientes están asimilados a códigos de error de AFT
#define LEGACY_BONUS_NO_RESPONSE        AFT_NO_RESPONSE        // 0xE1
#define LEGACY_BONUS_BAD_RESPONSE       AFT_BAD_RESPONSE       // 0xE2
#define LEGACY_BONUS_SERVER_NO_RESPONSE AFT_SERVER_NO_RESPONSE // 0xE3

typedef struct __attribute__((packed))
{
  uint8_t   vgmAddr;
  uint8_t   command;           // 0x90
  uint8_t   multiplier;
  uint8_t   multiplied_win[4]; // monto de premio multiplicado, en créditos (8 BCD)
  uint8_t   tax_status;        // 0x00: deducible - 0x01: no deducible - 0x02: wager watch
  uint8_t   bonus_amount[4];   // monto del bonus, en créditos (8 BCD)
  uint16_t  crc;
} LegacyBonusWinInfoType;

/*-----------------------------------------------
 * Llena con ceros el área de memoria indicada
 *-----------------------------------------------*/
void fillZeros(uint8_t *p, int n)
{
  int i;
  for(i=0; i<n; ++i)
    p[i] = 0;
}

/**
 * @brief Función encargada de manejar la carga de créditos a la VGM
 * @param None
 * @return None
 */
void cashin()
{
  int len, i;
  uint8_t status=0;
  char txt[16];
  uint32_t t0;

  LCD_print(_ESPERE, _PROCESANDO);

  // Prepara el mensaje para pedir crédito al servidor.

  fillZeros((uint8_t *)&cashMsgBuffer, sizeof(cashMsgBuffer));  // Inicializa con ceros el buffer de mensaje

  cashMsgBuffer.msgCode = CASHIN_REQ;
  memcpy( cashMsgBuffer.ID.cardID, cardID, 32 );
  if( features.teclado != TECLADO_NO )
  {
    // El módulo tiene PC con touch screen --> el vendedor carga el importe manualmente
    cashMsgBuffer.cashinOption = CASHIN_TOUCH;
  }
  else
  {
    // Módulo básico --> procedimiento de carga estándar
    cashMsgBuffer.cashinOption = CASHIN_STD;
  }
  cashMsgBuffer.protocol = 0;

  // Informa los valores de los contadores coin in, coin out, jackpot y total handpay cancelled credits,
  // drop y electronics transfers in y out.
  // Primero rellena con el valor por defecto (FF)
  for(i=0; i<4; ++i)
    cashMsgBuffer.ID.drop[i]=0xff;
  for(i=0; i<24; ++i)
    cashMsgBuffer.ID.coin_in[i]=0xff;

  if( (len = longPollBuffer.lp6f00[0]) )
    memcpy( cashMsgBuffer.ID.coin_in, longPollBuffer.lp6f00+len-3, 4 );    // coin in
  if( (len = longPollBuffer.lp6f01[0]) )
    memcpy( cashMsgBuffer.ID.coin_out, longPollBuffer.lp6f01+len-3, 4 );   // coin in
  if( (len = longPollBuffer.lp6f24[0]) )
    memcpy( cashMsgBuffer.ID.drop, longPollBuffer.lp6f24+len-3, 4 );       // drop
  if( (len = longPollBuffer.lp6f02[0]) )
    memcpy( cashMsgBuffer.ID.jackpot, longPollBuffer.lp6f02+len-3, 4 );    // jackpot
  if( (len = longPollBuffer.lp6f17[0]) )
    memcpy( cashMsgBuffer.ID.electronic_transfers_in, longPollBuffer.lp6f17+len-3, 4 );   // total EFT to VGM
  if( (len = longPollBuffer.lp6f18[0]) )
   memcpy( cashMsgBuffer.ID.electronic_transfers_out, longPollBuffer.lp6f18+len-3, 4 );   // total EFT to host
  if( (len = longPollBuffer.lp6f03[0]) )
    memcpy( cashMsgBuffer.ID.total_handpay_cancelled_credits, longPollBuffer.lp6f03+len-3, 4 );   // total hand paid cancelled credits

  // En el pedido de carga informa el monto acumulado de cargas de un
  // vendedor. Va en cero, excepto que la carga anterior haya sido
  // efectuada por un vendedor.
  sprintf( ( char * )cashMsgBuffer.monto_vendedor, "%06lu", montoVendedor );

  // Informa el valor de total credits from bills accepted
  for(i=0; i<4; ++i)
    cashMsgBuffer.total_credits_from_bills_accepted_cashin[i]=0xff;
  if( (len = longPollBuffer.lp6f0b[0]) )
    memcpy( cashMsgBuffer.total_credits_from_bills_accepted_cashin, longPollBuffer.lp6f0b+len-3, 4 );

  prepareCashMsgBuffer();  // Carga mac, uid, hash y crc. Luego cifra el mensaje

  // Envía requerimiento al servidor
  sendUDP( (uint8_t *) &cashMsgBuffer, sizeof( cashMsgBuffer ), upcb_cli, &addr_serv_cashless, SERV_PORT_CASHLESS );

  flag.waitingCashinReply = 1;

  // Espera recibir la respuesta al pedido de crédito o bien un timeout
  t0 = HAL_GetTick();
  while( flag.waitingCashinReply && (HAL_GetTick()-t0)<_3seg )
  {
    MX_LWIP_Process();
    delay(pollingPeriod);
    UART0_tx_80();
  }

  if ( !flag.waitingCashinReply )
  {
    if ( cashRxMsg.msgCode == CASHIN_REP_REJ )
    {
      /* Carga rechazada por el servidor. Pone un mensaje de error en el display
       y sale. Obs.: falta discriminar por tipo de error */
      switch( cashRxMsg.errorCode )
      {
      case CASH_TARJETA_INVALIDA:
        LCD_print( _ERROR, _TARJETA_INVALIDA );
        break;
      case CASH_TARJETA_BLOQUEADA:
        LCD_print( "TARJ. BLOQUEADA", _RETIRE_TARJETA );
        break;
      case CASH_ASOCIADA_A_OTRO_SLOT:
        LCD_print( "ERROR: TARJETA", "ASOC A OTRA GM" );
        break;
      case CASH_MONTO_INVALIDO:
        LCD_print( _ERROR_MONTO, "INVALIDO" );
        break;
      case CASH_MONTO_EXCESIVO:
        LCD_print( _ERROR_MONTO, "EXCESIVO" );
        break;
      case CASH_CARGA_DIARIA_EXCEDIDA:
        LCD_print( "ERROR: CARGA", "DIARIA EXCEDIDA" );
        break;
      case CASH_MAQUINA_BLOQUEADA:
        LCD_print( "ERROR: MAQUINA", "BLOQUEADA" );
        break;
      case CASH_MAQUINA_NO_PERMITIDA:
        LCD_print( "ERROR: MAQUINA", "NO PERMITIDA" );
        break;
      default:
        LCD_print ( _ERROR, _RETIRE_TARJETA );
        break;
      }
      monto = 0;
      cash_flag = 0;
      return;
    }
    else if ( cashRxMsg.msgCode == CASHIN_REP_CLI_OK )
    {
#if 0
      // Si la tarjeta actual es de cliente, para la próxima espera el tiempo más largo
      cardReaderLockTime = ( cfg.eibe )?_10seg:_5seg;
#endif
      // Verifica que la máquina no esté bloqueada
      if( flag.lockVGM )
      {
        // La máquina está bloqueada -> rechaza la carga
        LCD_print( _CARGA_INVALIDA, _MAQUINA_BLOQUEADA );

        // Envía el mensaje DE NAK al servidor
        sendCashinNak(AFT_TRANSFER_CANCELLED_BY_HOST);
        return;
      }

      // Intenta cargar la máquina

      // Si es una EIBE, fuerza a usar AFT
      if ( cfg.eibe )
        cashRxMsg.protocol = 'A';

      // Guarda el protocolo cashless a usar
      cashProtocol = cashRxMsg.protocol;

      /* Obtiene el monto a acreditar (de acuerdo a la VGM, este monto
       * puede estar dado en centavos o en créditos). Para AFT viene en centavos */
      monto = strtoul (cashRxMsg.monto, 0, 10);

      if ( !getCardStatus () )
      {
        /* Se retiró la tarjeta mientras se esperaba el cashin reply -->
         * --> no hace la carga y la reporta como errónea */
        status = AFT_TRANSFER_CANCELLED_BY_HOST;
        LCD_print_maquina_libre();
        cash_flag = 0;
        return;
      }
      else
      {
        // Intenta cargar la VGM
        // Si montoVendedor != 0 significa que la carga de cliente es posterior
        // a la compra a un vendedor. En ese caso muestra un mensaje para indicar
        // que la venta se ha concretado.
        if( montoVendedor )
        {
          // Muestra en el display el total que lleva cargado.
          sprintf( txt, "TOTAL: %-lu", montoVendedorPesos );
          LCD_print( "VENTA EFECTUADA", txt );
          delay(_2seg);
        }

        if ( monto + montoVendedor )
        {
          if ( cashProtocol == 'A' )
            status = AFT_cashin (monto + montoVendedor);
          else if ( cashProtocol == 'E' )
            // Por ahora no implementada la carga por EFT --> la rechaza
            status = AFT_UNABLE_TO_PERFORM_TRANSFER;  //EFT_cashin (monto + montoVendedor);
          else
            status = AFT_UNABLE_TO_PERFORM_TRANSFER;
        }
        else
        {
          status = AFT_FULL_TRANSFER_OK;
        }
      }
    }
    else  // CASHIN_REP_VEND_OK
    {
#if 0
      // Por ahora no implementada la carga de vendedor --> la rechaza
      status = AFT_TRANSFER_CANCELLED_BY_HOST;
      LCD_print("CARGA VENDEDOR", "NO SOPORTADA");
      delay(_2seg);
#else
      // Si es una carga de vendedor no carga la VGM, sino que acumula en
      // montoVendedor y envía un mensaje de ACK (simulando una carga exitosa)
      status = AFT_FULL_TRANSFER_OK;

      monto = strtoul (cashRxMsg.monto, 0, 10);
      cargaUnitVendedor = strtoul( cashRxMsg.ID.montoPesos,0,10 );

      // Acumula el monto de la venta
      montoVendedor += monto;
      montoVendedorPesos += cargaUnitVendedor;

      // Muestra el acumulado a cargar a la VGM
      sprintf( txt, "CARGA: %-lu", montoVendedorPesos );
      LCD_print( txt, _RETIRE_TARJETA );

      flag.cardStatus = 0;
      cash_flag = 0;
      sendCashinAck();
      return;
#endif
#if 0
      // Si es una carga de vendedor no carga la VGM, sino que acumula en
      // montoVendedor y envía un mensaje de ACK (simulando una carga exitosa)
      status = AFT_FULL_TRANSFER_OK;
      sendCashinAck();

      // Si la tarjeta actual es de vendedor, para la próxima espera un tiempo más breve.
      cardReaderLockTime = _10ms;

      // Acumula el monto de la venta
      montoVendedor += monto;
      montoVendedorPesos += cargaUnitVendedor;
#endif
    }
    sendCashinAckNak (status);
  }
  else
  {
    sendCashinAckNak( (status = AFT_TRANSFER_FAILED) );
    LCD_print("ERROR DE COMUNIC",_RETIRE_TARJETA);
    cash_flag = 0;
    return;
  }

  if( !status )
  {
    LCD_print( _MAQUINA_JUGANDO, _BUENA_SUERTE );
#if 0
    // La carga en la VGM fué exitosa. Guarda en cashin_ok el código del mensaje de
    // respuesta del servidor (CASHIN_REP_CLI_OK o CASHIN_REP_VEND_OK), para saber
    // si en el cashout debe descargar la VGM o no.
    // Obs.: cuando se retira la tarjeta de un vendedor no se descargan los créditos
    cashin_ok = cashRxMsg.msgCode;
#endif
    if( cashRxMsg.msgCode == CASHIN_REP_CLI_OK )
    {
      // Por si antes de la carga de cliente hubo una de vendedor,
      // pone a cero el monto de venta
      montoVendedor = 0;
      montoVendedorPesos = 0;
    }
  }else
    LCD_print( "ERROR EN CARGA", _RETIRE_TARJETA );

  // Resetea el flag de requerimiento de cashless
  cash_flag = 0;
}

/**
 * @brief Prepara un mensaje cargado sobre cashMsgBuffer, para poder ser enviado.
 *        Carga los campos comunes a todos (mac, uid, hash y crc) y luego cifra
 *        el mensaje
 */
void prepareCashMsgBuffer()
{
  int j;

  for( j=0; j<3; ++j )
    cashMsgBuffer.mac[j] = MyMAC[j];
  cashMsgBuffer.uid = uid;
  cashMsgBuffer.hash = rand();
  cashMsgBuffer.crc = CRC16( ( uint8_t * )&cashMsgBuffer, sizeof( cashMsgBuffer )-2, 0 );
  cifrar( ( uint8_t * )&cashMsgBuffer,sizeof( cashMsgBuffer ) );
}
/**
 * @brief Carga los campos de un mensaje de handpay que son comunes también a
 *        los mensajes de reporte de pago de premio progresivo y legacy bonus.
 */
void prepareHandpayMsgBuffer()
{
  //  int i;

  cashMsgBuffer.msgCode = CASHOUT_HANDPAY_REQ;  // Se reporta como si fuera un pago manual
  cashMsgBuffer.protocol = cashProtocol;
  cashMsgBuffer.errorCode = 0;
#if REVISAR
    for(i=0; i<28; ++i)
      cashMsgBuffer.lp1c[i] = longPollBuffer.lp1c[i];
    for(i=0; i<4; ++i)
      cashMsgBuffer.lp10[i] = longPollBuffer.lp10[i];
  #endif // REVISAR
  sprintf (cashMsgBuffer.datetime, "%04hu",      (uint16_t) tiempo.anio);
  sprintf (cashMsgBuffer.datetime + 4, "%02hu",  (uint16_t) tiempo.mes);
  sprintf (cashMsgBuffer.datetime + 6, "%02hu",  (uint16_t) tiempo.dia);
  sprintf (cashMsgBuffer.datetime + 8, "%02hu",  (uint16_t) tiempo.hora);
  sprintf (cashMsgBuffer.datetime + 10, "%02hu", (uint16_t) tiempo.min);
  sprintf (cashMsgBuffer.datetime + 12, "%02hu", (uint16_t) tiempo.seg);
}

/**
 * @brief Realiza una descarga cashless y transfiere el monto al servidor
 * @param None
 * @return None
 */
void cashout()
{
  int len, i;
  uint32_t cash_monto=0, t0;

  if( flag.waitingEvent52 )
    /* Hay un pago manual en proceso --> se debe esperar hasta que lo termine de procesar,
       antes de realizar un cashout */
    return;

  fillZeros((uint8_t *)&cashMsgBuffer, sizeof(cashMsgBuffer));  // Inicializa con ceros el buffer de mensaje

  // Descarga la máquina (y obtiene el código de error a enviar en el mensaje)
  if( cashProtocol=='A' || cashProtocol=='a')
  {
    // Intenta el cashout
    if( flag.cashoutWin )
    {
      // Descarga de un jackpot en respuesta a un evento 6B
      cashMsgBuffer.errorCode = AFT_cashout_win( &cash_monto );

      if( cashMsgBuffer.errorCode != AFT_PARTIAL_TRANSFER_OK )
      {
        /* Puede ser que la descarga no se haya podido concretar por haber
           un pago manual pendiente que lo impida.
           Retorna sin hacer nada para permitir verificar los eventos, y en
           todo caso volverá a intentar la descarga en la próxima vuelta. */
        return;
      }
    }
    else
    {
      // Descarga AFT por retiro de tarjeta o evento 6A
      cashMsgBuffer.errorCode = AFT_cashout( &cash_monto );

      if( cashMsgBuffer.errorCode == AFT_FULL_TRANSFER_OK )
      {
        // Va a enviar el mensaje al servidor, pero antes verifica si se trata
        // de una descarga en medio de una partida, para la recarga de crédito
        // para bola extra. Para esto usa como código de error el código del
        // evento de inicio de jugada (7E).
        if(flag.VGMjugando && flag.enableBolaExtra)
        {
          cashMsgBuffer.errorCode = EVT_7E_GAME_STARTED;
        }
      }
      else
      {
        if(cashMsgBuffer.errorCode == AFT_NO_RESPONSE && cashProtocol=='a')
        {
          // Está descargando, porque en el inicio del módulo la máquina tenía
          // créditos. Como no respondió al intento de descarga EFT, se pasa a ECT
          // para el próximo intento.
          // Obs.: 'a', 'e' y 'c' se usan para probar AFT, EFT y ECT, respectivamente.
          // Cuando se tiene certeza del protocolo a usar se usa 'A', 'E' o 'C'.
          cashProtocol = 'e';
        }
        // Puede ser que la descarga no se haya podido concretar por haber
        // un pago manual pendiente que lo impida.
        // Retorna sin hacer nada para permitir verificar los eventos, y en
        // todo caso volverá a intentar la descarga en la próxima vuelta.
        return;
      }
    }
  }
  else if( cashProtocol=='E' || cashProtocol=='e' || cashProtocol=='C' || cashProtocol=='c' )
  {
#if 1
    // Por ahora no implementado EFT --> hace una descarga en cero
    cashMsgBuffer.errorCode = EFT_SUCCESS;
    cash_monto = 0;
#else
    // Antes de descargar consulta si la VGM tiene crédito
    if( !getCurrentCredits( &currentCredits ) )
    {
      // La VGM no respondió a la consulta de créditos --> aborta
      // la descarga para volver a intentar un tiempo después.
      return;
    }
    cash_monto = currentCredits;
    if( cash_monto )
    {
      // Intenta el cashout
      cashMsgBuffer.errorCode = EFT_cashout( ((cashProtocol=='E'||cashProtocol=='e')?0x64:0x24));

      if( cashMsgBuffer.errorCode == EFT_SUCCESS )
      {
        // Guarda en monto64 el monto descargado. Esto lo hace porque, por errores
        // en el software de algunas VGM, el LP64 informa que descargó y en realidad
        // la VGM queda cargada.
        if( !monto64 )
          monto64 = cash_monto;

        // Verifica que la VGM haya quedado descargada (esto es porque en las WMS
        // cuando se descarga con el botón de jugar apretado, aparenta descargar
        // pero deja los créditos en la VGM.
        // También, en Atronic sucede que la VGM demora en poner a cero el contador
        // de créditos luego de una descarga normal.
        // Por lo que se vió en algunas capturas, la VGM demora en poner a cero
        // el contador de créditos
        for( aux=0; aux<25 && currentCredits; ++aux )
        {
          delay( pollingPeriod );
          UART0_tx(0x80,1);
          delay( pollingPeriod );
          getCurrentCredits( &currentCredits );
        }
        if( !currentCredits )
        {
          // Sale para enviar el mensaje de cashout al servidor
          monto64 = 0;
        }
        else
        {
          // La VGM no se descargó. Guarda el monto obtenido con el LP64 para
          // intentar más tarde la descarga
          return;
        }
      }
      else if(cashMsgBuffer.errorCode == EFT_NO_RESPONSE && cashProtocol=='e')
      {
        // Está descargando, porque en el inicio del módulo la máquina tenía
        // créditos. Como no respondió al intento de descarga EFT, se pasa a ECT
        // para el próximo intento.
        // Obs.: 'a', 'e' y 'c' se usan para probar AFT, EFT y ECT, respectivamente.
        // Cuando se tiene certeza del protocolo a usar se usa 'A', 'E' o 'C'.
        cashProtocol = 'c';
        return;
      }
      else if(cashMsgBuffer.errorCode == EFT_NO_RESPONSE && cashProtocol=='c')
      {
        // Está descargando, porque en el inicio del módulo la máquina tenía
        // créditos. Como no respondió al intento de descarga EFT, se pasa a ECT
        // para el próximo intento.
        // Obs.: 'a', 'e' y 'c' se usan para probar AFT, EFT y ECT, respectivamente.
        // Cuando se tiene certeza del protocolo a usar se usa 'A', 'E' o 'C'.
        cashProtocol = 'a';
        return;
      }
      else
      {
        // Falló la descarga porque la VGM estaba ocupada, jugando o en otra condición.
        // Retorna sin hacer nada para permitir verificar los eventos, y en
        // todo caso volverá a intentar la descarga en la próxima vuelta.
        return;
      }
    }
    else
    {
      // La VGM no tiene crédito --> saltea la descarga de la máquina
      // y va directamente a enviar el mensaje al servidor
      cashMsgBuffer.errorCode = EFT_SUCCESS;
      cash_monto = monto64;
      monto64 = 0;
    }
#endif
  }
  else
  {
    // No está registrado un protocolo válido para descargar --> anula el pedido de descarga y sale
    cash_flag = 0;
    return;
  }

  // Prepara el mensaje a enviar al servidor
  if( flag.cashoutWin )
  {
    /* La descarga fue a raiz de un evento 6B, entonces para que no desasocie la máquina
       la reporta como pago manual, ya que no hay un código específico para este tipo
       de cashout. */
    cashMsgBuffer.msgCode = CASHOUT_HANDPAY_REQ;
    cashMsgBuffer.progLevel = 1; /* Esto es para que el servidor interprete que el monto
                                    está en centavos y no en unidades de denominación, ya que
                                    si bien fue una descarga por AFT se reporta como pago
                                    manual para que no desasocie la máquina */
  }
  else
    cashMsgBuffer.msgCode = CASHOUT_REQ;

  cashMsgBuffer.protocol = cashProtocol;

  /* Luego de la descarga hace LP 2F y LP 6F para tener valores actualizados de
     coin in, coin out, jackpot y total handpay cancelled credits, posteriores a la descarga. */
  /* Como algunas máquinas (por ej. Zitro) demoran en actualizar el contador de EFT OUT
     después de una descarga, agrega un retardo antes de hacer los LP. */
  delay80(_500ms);

  delay(pollingPeriod);
  polling(NPOLL_TOTAL_TICKETS_IN_OUT);

  delay(pollingPeriod);
  UART0_tx_80();

  delay(pollingPeriod);
  polling(NPOLL_COIN_IN);

  delay(pollingPeriod);
  UART0_tx_80();

  // Primero rellena con el valor por defecto (FF)
  for(i=0; i<28; ++i)
    cashMsgBuffer.lp1c[i]=0xff;
  for(i=0; i<12; ++i)
    cashMsgBuffer.lp2f[i]=0xff;
  for(i=0; i<4; ++i)
    cashMsgBuffer.total_credits_from_bills_accepted_cashout[i]=0xff;

  if( (len = longPollBuffer.lp6f00[0]) )
    memcpy( cashMsgBuffer.lp1c, longPollBuffer.lp6f00+len-3, 4 );     // coin in
  if( (len = longPollBuffer.lp6f01[0]) )
    memcpy( cashMsgBuffer.lp1c+4, longPollBuffer.lp6f01+len-3, 4 );   // coin in
  if( (len = longPollBuffer.lp6f24[0]) )
    memcpy( cashMsgBuffer.lp1c+8, longPollBuffer.lp6f24+len-3, 4 );   // drop
  if( (len = longPollBuffer.lp6f02[0]) )
    memcpy( cashMsgBuffer.lp1c+12, longPollBuffer.lp6f02+len-3, 4 );  // jackpot

  memcpy( cashMsgBuffer.lp1c+16, longPollBuffer.lp1c, 12 );           // games played / games won / door open

  if( (len = longPollBuffer.lp6f17[0]) )
    memcpy( cashMsgBuffer.lp2f, longPollBuffer.lp6f17+len-3, 4 );     // total EFT to VGM
  if( (len = longPollBuffer.lp6f18[0]) )
    memcpy( cashMsgBuffer.lp2f+4, longPollBuffer.lp6f18+len-3, 4 );   // total EFT to host

  if( (len = longPollBuffer.lp6f03[0]) )
    memcpy( cashMsgBuffer.lp2f+8, longPollBuffer.lp6f03+len-3, 4 );   // total hand paid cancelled credits

  if( (len = longPollBuffer.lp6f0b[0]) )
    memcpy( cashMsgBuffer.total_credits_from_bills_accepted_cashout, longPollBuffer.lp6f0b+len-3, 4 );

  sprintf( cashMsgBuffer.datetime,   "%04hu", ( uint16_t )tiempo.anio);
  sprintf( cashMsgBuffer.datetime+4, "%02hu", ( uint16_t )tiempo.mes);
  sprintf( cashMsgBuffer.datetime+6, "%02hu", ( uint16_t )tiempo.dia);
  sprintf( cashMsgBuffer.datetime+8, "%02hu", ( uint16_t )tiempo.hora);
  sprintf( cashMsgBuffer.datetime+10,"%02hu", ( uint16_t )tiempo.min);
  sprintf( cashMsgBuffer.datetime+12,"%02hu", ( uint16_t )tiempo.seg );

  // El monto a reportar puede provenir de lo que tuviera la VGM (cash_monto) o
  // de lo que hubiera estado cargando un vendedor (montoVendedor), cuando sale por
  // timeout de espera de tarjeta de cliente.
  if( montoVendedor )
    sprintf( ( char * )cashMsgBuffer.monto, "%010lu", montoVendedor );
  else
    sprintf( ( char * )cashMsgBuffer.monto, "%010lu", cash_monto );

  prepareCashMsgBuffer(); // Carga mac, uid, hash y crc. Luego cifra el mensaje

  while(1)
  {
    // Envía mensaje al servidor, para que acredite el monto descargado
    sendUDP( ( unsigned char * )&cashMsgBuffer, sizeof( cashMsgBuffer ), upcb_cli, &addr_serv_cashless, SERV_PORT_CASHLESS );

    flag.waitingCashoutReply = 1;

    // Espera recibir la respuesta al pedido de crédito o bien un timeout
    t0 = HAL_GetTick();
    while( flag.waitingCashoutReply && (HAL_GetTick()-t0)<_3seg )
    {
      MX_LWIP_Process();
      delay(pollingPeriod);
      UART0_tx_80();
    }

    if ( !flag.waitingCashoutReply )
    {
      if ( cashRxMsg.msgCode == CASHOUT_REP_OK )
      {
        cash_flag = 0;
        flag.waitingCashoutReply = 0;

        // Si viene algún saldo en el mensaje, lo muestra durante 3 segundos
        if( cashRxMsg.saldoFinal[0] )
        {
          char txt[20];
          sprintf( txt, "SALDO:%-.9s", cashRxMsg.saldoFinal );
          LCD_print( "DESCARGA OK", txt );
          delay( _3seg );
          UART0_tx_80();
          if( flag.cashoutWin )
          {
            flag.cashoutWin = 0;
            LCD_print( _MAQUINA_JUGANDO, _BUENA_SUERTE );
          }else{
            while ( getCardStatus() )
            {
              LCD_print( "RETIRE", "TARJETA" );
              // por si demora mucho tiempo antes de retirar la tarjeta,
              // si tiene eventos en el buffer los transmite
              if (!waitingAnyServerReply() && bufEventos.nEventos)
                sendEvents();  // Envía los eventos al servidor y se bloquea hasta recibir el ACK

              delay80(_500ms);
            }
            LCD_print(_MAQUINA_LIBRE, _INGRESE_TARJETA);
          }
        }else{
          if( flag.cashoutWin )
          {
            flag.cashoutWin = 0;
            LCD_print( _MAQUINA_JUGANDO, _BUENA_SUERTE );
          }else{
            LCD_print(_MAQUINA_LIBRE, _INGRESE_TARJETA);
          }
        }
        break;
      }
      else
      {
        // Condición inesperada --> Debe detenerse la ejecución acá y mostrar
        // el monto descargado que fue rechazado por el servidor
        while(1)
        {
          LCD_print( "CASHOUT_REP_REJ", _MAQUINA_BLOQUEADA );

          delay(pollingPeriod);
          UART0_tx_80();
        }
      }
    }
  }
  // Por las dudas resetea el monto acumulado
  // de la carga del último vendedor
  montoVendedor = 0;
//  montoVendedorPesos = 0;

  // Resetea el flag de requerimiento de cashless
  cash_flag = 0;
}

/**
 * @brief Resetea un pago manual pendiente y transfiere el monto al servidor
 * @param
 * @return
 */
//uint8_t reset_handpay( uint8_t evt )
//{
//  return 1;
//}
/**
 * @brief Procesa un mensaje recibido de respuesta de cashless o pago manual
 * @param udp_buf Puntero al mensaje UDP recibido
 * @return None
 */
void processCashlessReply (struct pbuf *udp_buf)
{
  // Copia el mensaje recibido para su posterior procesamiento

  memcpy ((void*) &cashRxMsg, udp_buf->payload, udp_buf->len);

  decifrar((uint8_t* )&cashRxMsg, udp_buf->len);

  if ( CRC16 ((uint8_t*) &cashRxMsg, udp_buf->len, 0) == 0 && udp_buf->len == sizeof(CashMsgType) )
  {
    if ( flag.waitingCashinReply )
    {
      if ( flag.waitingCashinNakEnd )
      {
        if ( cashRxMsg.msgCode == CASHIN_NAK_END )
        {
          flag.waitingCashinReply = 0;
          flag.waitingCashinNakEnd = 0;
        }
      }
      else
      {
        if ( cashRxMsg.msgCode == CASHIN_REP_CLI_OK || cashRxMsg.msgCode == CASHIN_REP_VEND_OK || cashRxMsg.msgCode == CASHIN_REP_REJ )
          flag.waitingCashinReply = 0;
      }
    }
    else if ( flag.waitingCashoutReply )
    {
      if ( cashRxMsg.msgCode == CASHOUT_REP_OK || cashRxMsg.msgCode == CASHOUT_REP_REJ )
        flag.waitingCashoutReply = 0;
    }
    else if ( flag.waitingHandpayReply )
    {
      if ( cashRxMsg.msgCode == CASHOUT_REP_OK )
        flag.waitingHandpayReply = 0;
    }
  }
}

/**
 * @brief Envía CASHIN_ACK o CASHIN_NAK al servidor de cashless.
 *        CASHIN_NAK se usa para deshacer una eventual carga de créditos registrada
 *        por el servidor de cashless, debido a que la VGM rechazó la carga o bien que
 *        no se recibió el mensaje de CASHIN_REP... por parte del servidor
 */
void sendCashinAckNak (uint8_t errorCode)
{
  uint32_t t0;

  if ( !errorCode )
  {
    flag.waitingCashinNakEnd = 0;
    flag.waitingCashinReply = 0;
  }
  else
  {
    flag.waitingCashinNakEnd = 1;        // Señaliza que está esperando CASHIN_NAK_END del servidor
    flag.waitingCashinReply = 1;
  }

  do
  {
    cashMsgBuffer.msgCode = ((errorCode) ? CASHIN_NAK : CASHIN_ACK);
    cashMsgBuffer.errorCode = errorCode;
    cashMsgBuffer.protocol = cashProtocol;

    if ( cashMsgBuffer.protocol != 'T' )
    {
      // En los CASHIN_NAK de cashless, por las dudas pone en cero el ID de tarjeta y el monto,
      // ya que no se usan en este tipo de mensajes.
      // En cambio, en los CASHIN_NAK de TITO, deja los datos del ticket, para que el servidor pueda
      // devolverle el estado al ticket cuya carga falló.
      long2str ((char*) cashMsgBuffer.ID.cardID, 0, 31);
      //        sprintf( ( char * )cashMsgPtr->ID.cardID, "0000000000000000000000000000000" );
    }
    long2str (cashMsgBuffer.monto, 0, 10);
    //      sprintf( ( char * )cashMsgPtr->monto, "%d", 0 );

    prepareCashMsgBuffer();      // Carga mac, uid, hash y crc. Luego cifra el mensaje

    // Envía requerimiento al servidor
    sendUDP( (uint8_t *) &cashMsgBuffer, sizeof(CashMsgType), upcb_cli, &addr_serv_cashless, SERV_PORT_CASHLESS);

    if ( flag.waitingCashinNakEnd )
    {
      // Espera recibir CASHIN_NAK_END para finalizar el proceso
      t0 = HAL_GetTick();
      while( flag.waitingCashinNakEnd && (HAL_GetTick()-t0)<_3seg )
      {
        MX_LWIP_Process();
        delay(pollingPeriod);
        UART0_tx_80();
      }

      if ( !flag.waitingCashinNakEnd )
      {
        flag.waitingCashinNakEnd = 0;
        flag.waitingCashinReply = 0;
      }
    }
  }
  while (flag.waitingCashinNakEnd);
}

#if 0
/**
 * @brief Transacción AFT para cargar crédito en la VGM
 * @param monto a cargar (en centavos)
 * @return 0 si AFT_FULL_TRANSFER_OK
 *         Transfer Status si no se completó la transferencia
 *         0x0F si VGM no responde
 *         0x2F si error en AFT Lock
 *         0x1F si otra causa de error
 */

static uint8_t AFT_cashin( uint32_t monto )
{
  // Falta implementar --> ahora solo a modo de prueba
  if( monto <= 1000 )
    return AFT_FULL_TRANSFER_OK;
  else
    return AFT_TRANSFER_FAILED;
}

/**
 * @brief Transacción AFT para descargar todo el crédito de la VGM
 * @param None
 * @return  0 (AFT_FULL_TRANSFER_OK)
 *          o el Transfer Status que indica por que no se completó la transferencia
 *          o 0x0F (la VGM no responde)
 *          o 0x2F (Error en AFT Lock)
 *          o 0x1F (otra causa de error)
 *          La variable cash_monto queda cargada con el monto descargado
 */
static uint8_t AFT_cashout( uint32_t * pMonto )
{
  *pMonto = monto;
  return AFT_FULL_TRANSFER_OK;
}
#endif

//--------------------------------------------
// Envía un AFT status request, para obtener
// los montos cashable y non-restricted
// cmd : 0x00 lock request
//       0x80 cancel lock
//       0xff status request
// Retorna 0x00 : Lock OK
//         0x40 : Lock pendiente
//         0xff : no bloqueado
//--------------------------------------------

#define pr ((AFT74Type *)rx_buffer)

static uint16_t AFT_lock_status_request (uint8_t cmd, uint8_t transfer_condition, uint32_t *pMonto)
{
  uint16_t i;

  tx_buffer[0] = vgmAddr;
  tx_buffer[1] = 0x74;
  tx_buffer[2] = cmd;                 // comando
  tx_buffer[3] = transfer_condition;  // transfer condition (VGM->host o host->VGM)
  tx_buffer[4] = 0x05;                // timeout 500 centésimas de segundo (en BCD) (MSB)
  tx_buffer[5] = 0x00;                // (LSB)
  i = CRC16 (tx_buffer, 6, 0);
  tx_buffer[6] = i & 0x00ff;
  tx_buffer[7] = (i >> 8) & 0x00ff;

  // transmite
  UART0_tx (tx_buffer, 8);

  // Espera respuesta (hasta 5 veces el tiempo de respuesta)
  if ( !UART0_rx() )
    return AFT_NO_RESPONSE;
  else if ( !valid_AFT_response (0x74) )
    return AFT_BAD_RESPONSE;

  // Obtiene el asset number que luego va a usar para la transacción AFT
  for(i=0; i<4; ++i)
    AFT_asset_number[i] = pr->resp.assetNumber[i];

  // Obtiene el monto cashable que tiene la VGM. Si el lock se
  // hace antes de una descarga, éste es el monto a descargar
  *pMonto = bcd2long ( pr->resp.cashableAmount, 10);

  return pr->resp.gameLockStatus;
}

#undef pr

//-----------------------------------------------------------
// Verifica que una respuesta AFT sea válida en general.
// Posteriormente se deben verificar cuestiones específicas,
// de acuerdo a cada caso.
// Retorna 1 (OK) o 0 (Error)
//-----------------------------------------------------------
static uint16_t valid_AFT_response (uint8_t cmd)
{
  if ( rx_buflen )                           // que el mensaje no sea nulo
    if ( CRC16 (rx_buffer, rx_buflen, 0) == 0 )  // que el CRC sea correcto
      if ( rx_buffer[0] == vgmAddr )         // que la dirección de la VGM sea la correcta
        if ( rx_buffer[1] == cmd )           // que el comando AFT sea el esperado
          if ( rx_buflen == (rx_buffer[2] + 5) )  // que la longitud indicada coincida con la real
            return 1;
  return 0;
}

//--------------------------------------------
// Envía un AFT interrogate
//--------------------------------------------
static void AFT_interrogate (void)
{
  uint16_t i;

  tx_buffer[0] = vgmAddr;
  tx_buffer[1] = 0x72;
  tx_buffer[2] = 2;
  tx_buffer[3] = 0xFF;  // complete transfer
  tx_buffer[4] = 0x00;  // transaction index
  i = CRC16 (tx_buffer, 5, 0);
  tx_buffer[5] = i & 0x00ff;
  tx_buffer[6] = (i >> 8) & 0x00ff;

  // transmite
  UART0_tx (tx_buffer, 7);
}

//-------------------------------------------------
// Prepara el mensaje AFT Init, para subir o bajar
// crédito, de acuerdo al tipo y monto solicitado
//-------------------------------------------------

#define pt ((AFT72Type *)tx_buffer)

static void AFT_prepare_init (uint8_t transferCode, uint8_t transferType, uint8_t flags, uint32_t monto)
{
  uint16_t i;

  tx_buflen = sizeof( pt->init);

  pt->init.vgmAddr = vgmAddr;
  pt->init.commandCode = 0x72;
  pt->init.length = tx_buflen - 5;
  pt->init.transferCode = transferCode;
  pt->init.transactionIndex = 0;
  pt->init.transferType = transferType;
  long2bcd ( pt->init.cashableAmount, monto, 10);
  long2bcd ( pt->init.restrictedAmount, 0, 10);
  long2bcd ( pt->init.nonRestrictedAmount, 0, 10);
  pt->init.transferFlags = flags;
  for(i=0; i<4; ++i)
    pt->init.assetNumber[i] = AFT_asset_number[i];
  for (i = 0; i < 20; ++i)
    pt->init.registrationKey[i] = 0;
  pt->init.transactionIDLength = 7;
  strcpy ((char*) pt->init.transactionIDtxt, "binsur");
  pt->init.transactionID = AFT_transactionID;
  for (i = 0; i < 4; ++i)
    pt->init.expiration[i] = 0;
  pt->init.poolID[0] = 0;
  pt->init.poolID[1] = 0;
  pt->init.receiptDataLength = 0;
  i = CRC16 ((uint8_t*) pt, tx_buflen - 2, 0);
  pt->init.crc[0] = i & 0x00ff;
  pt->init.crc[1] = (i >> 8) & 0x00ff;
}

#undef pt

//----------------------------------------------------------------------------------
// Hace una interrogación para cerrar una eventual transacción incompleta
// anterior, y obtener el número de transaction ID
// Si interrogate es cero, simplemente incrementa el valor actual, sin
// hacer la interrogación.
// Retorna: 1 si OK
//          0 si no pudo obtenerlo
//----------------------------------------------------------------------------------

#define pr ((AFT72Type *)rx_buffer)

uint8_t AFT_getTransactionID (uint16_t interrogate)
{
  if ( !interrogate )
  {
    // No interroga --> incremente el transactionID anterior
    AFT_transactionID = ((AFT_transactionID < '0' || AFT_transactionID > '8') ? '0' : AFT_transactionID + 1);
    return 1;
  }
  // Hace una interrogación para ver el ID de la última transacción
  AFT_interrogate ();

  // Espera respuesta (hasta 5 veces el tiempo de respuesta normal)
  // y verifica que la misma sea válida.
  if ( !UART0_rx() || !valid_AFT_response (0x72) || rx_buflen < 37 || pr->resp.transactionIDLength != 7 )
  {
    // No recibió respuesta válida a la interrogación --> pone
    // un número de transacción fuera de la secuencia normal
    AFT_transactionID = 'A' + (tiempo.seg % 20);
    return 0;
  }

  if ( ( pr->resp.transferStatus & AFT_TRANSFER_STATUS_MASK) == AFT_TRANSFER_OK )
  {
    // Obtiene el último ID para incrementarlo
    AFT_transactionID = (( pr->resp.transactionID < '0' || pr->resp.transactionID > '8') ? '0' : pr->resp.transactionID + 1);
    // Retorna el status de la última transacción AFT
  }
  else
  {
    // Pone un número de transacción fuera de la secuencia normal
    AFT_transactionID = 'A' + (tiempo.seg % 20);
  }
  return 1;
}

#undef pr

//----------------------------------------------------------------------------------
// Transacción AFT para cargar crédito en la VGM
// Retorna: 0 (AFT_FULL_TRANSFER_OK)
//          o el Transfer Status que indica por que no se completó la transferencia
//          o 0x0F (VGM no responde)
//          o 0x2F (Error en AFT Lock)
//          o 0x1F (otra causa de error)
//----------------------------------------------------------------------------------

#define pr ((AFT72Type *)rx_buffer)
#define wait69time i
#define wait6Ftime i
#define aftResponse j

static uint8_t AFT_cashin (uint32_t monto)
{
  uint32_t monto_actual;
  uint16_t i, j;
  uint8_t evt;

  i = 0;

  if ( !cfg.eibe )
  {
    // Intenta bloquear la VGM
    do
    {
      j = AFT_lock_status_request ( AFT_LOCK_REQUEST, AFT_LOCK_4_CASHIN, &monto_actual);

      delay ( pollingPeriod );
      UART0_tx_80();
      delay ( pollingPeriod );
    }
    while (!(j == AFT_LOCKED || j == AFT_LOCK_PENDING) && ++i < 5);

    // Espera el evento 6F (EVT_GAME_LOCKED)
    for (wait6FTimer = 0, wait6Ftime = _4seg;;)
    {
      j = getEvento (&evt);

      delay( pollingPeriod );

      if ( j )
      {
        if ( filtrarEvento (&evt) && bufEventos.nEventos < MAX_EVENTOS )
        {
          // Eventos que se reportan al servidor
          bufEventos.evento[bufEventos.nEventos++] = evt;

          // Hace un backup del evento
          //backupEvt(evt);
        }

        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM

        if ( evt == EVT_12_MAIN_DOOR_CLOSED )
          flag.doorOpen = 0;

        if ( evt == EVT_6F_GAME_LOCKED )
          break;
      }
      else
      {
        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM
      }
      if ( wait6Ftime && wait6FTimer > wait6Ftime )
        break;  // Permite continuar con la carga sin haber logrado el bloqueo de la VGM
    }
    delay ( pollingPeriod );
  }

  // Obtiene el transactionID a usar
  AFT_getTransactionID (1);

  delay ( pollingPeriod );
  UART0_tx_80();
  delay ( pollingPeriod );

  // Prepara el AFT init
  AFT_prepare_init ( AFT_FULL_TRANSFER_REQ,
                     AFT_CASHIN,
                     AFT_HOST_CASHOUT_ENABLE_CTL | AFT_HOST_CASHOUT_ENABLE | AFT_HOST_CASHOUT_MODE_HARD,
                     monto);

  // y lo envía por la UART0
  UART0_tx (tx_buffer, tx_buflen);

  // Espera respuesta
  if ( !UART0_rx() )
  {
    aftResponse = AFT_NO_RESPONSE;            // La VGM no respondió
  }
  else if ( !valid_AFT_response (0x72) )       // Verifica que la respuesta sea válida
  {
    // e indique transferencia pendiente
    aftResponse = AFT_BAD_RESPONSE;
  }
  else
  {
    aftResponse = pr->resp.transferStatus;
  }

  // En función de la respuesta determina cuanto tiempo debe esperar el evento 69.
  // En caso de que la VGM haya respondido con AFT_FULL_TRANSFER_OK no espera el evento 69
  // ni hace la interrogación final
  if ( aftResponse != AFT_FULL_TRANSFER_OK )
  {
    if ( aftResponse == AFT_TRANSFER_PENDING )
      wait69time = 0;         // Espera indefinidamente por el evento 69
    else
      wait69time = _3seg;     // La VGM no respondió o respondió erróneamente o la respuesta no
    // es la esperada. Por las dudas espera durante 3 segundos por el
    // evento 69, antes de dar por fallada la transacción.

    // Espera el evento 0x69 (AFT_TRANSFER_COMPLETE)
    for (wait69Timer = 0;;)
    {
      delay ( pollingPeriod );

      if ( getEvento (&evt) )
      {
        if ( filtrarEvento (&evt) && bufEventos.nEventos < MAX_EVENTOS )
        {
          // Eventos que se reportan al servidor
          bufEventos.evento[bufEventos.nEventos++] = evt;

          //          // Hace un backup del evento
          //          backupEvt(evt);
        }

        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM

        if ( evt == EVT_12_MAIN_DOOR_CLOSED )
          flag.doorOpen = 0;

        if ( evt == EVT_69_AFT_TRANSFER_COMPLETE )
          break;
      }
      else
      {
        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM
      }
      if ( wait69time && wait69Timer > wait69time )
        return aftResponse;
    }

    // Va a enviar la interrogación para terminar la transacción AFT
    do
    {
      delay ( pollingPeriod );
      UART0_tx_80();
      delay ( pollingPeriod );

      // Envía la interrogación con 0xFF para completar la transferencia
      AFT_interrogate ();
    }
    while (!UART0_rx() || !valid_AFT_response (0x72) || pr->resp.transferStatus == AFT_TRANSFER_PENDING);
  }
  // Retorna el transfer status de la última interrogación (o bien
  // el de la respuesta al init si la respuesta fue AFT_FULL_TRANSFER_OK
  return pr->resp.transferStatus;
}

#undef wait69time
#undef wait6Ftime
#undef aftResponse
#undef pr

/* Convierte el monto de current credits a centavos, multiplicando por el valor
   de denominación que tiene configurada la máquina, que se obtiene con el LP1F.
   Guarda el monto en cash_monto.
   Retorna 0 si OK o código de error.
*/
static unsigned int currentCredits2cents(uint32_t *pMonto)
{
  if( getCurrentCredits(&currentCredits) )
  {
    *pMonto = unidad_denominacion * currentCredits;
    return 0;
  }
  *pMonto = 0;
  return AFT_UNEXPECTED_ERROR;
}

//----------------------------------------------------------------------------------
// Transacción AFT para descargar todo el crédito de la VGM
// Retorna: 0 (AFT_FULL_TRANSFER_OK)
//          o el Transfer Status que indica por que no se completó la transferencia
//          o 0x0F (la VGM no responde)
//          o 0x2F (Error en AFT Lock)
//          o 0x1F (otra causa de error)
//          La variable cash_monto queda cargada con el monto descargado
//----------------------------------------------------------------------------------

#define pr ((AFT72Type *)rx_buffer)
#define wait69time i
#define wait6Ftime i
#define aftResponse j

static uint8_t AFT_cashout (uint32_t *pMonto)
{
  unsigned int i, j;
  unsigned char evt;
  unsigned long lock_cashout_monto = 0;

  i = j = *pMonto = 0;

  if(features.cashless & NO_LOCK_AFT_CASHOUT)
  {
    // Descarga sin bloqueo AFT
    if( features.cashless & AFT_CASHOUT_LP1A )
    {
      /* Obtiene el monto a descargar de current credits, porque en algunas máquinas (ruletas
         Electrochance), la consulta de AFT Lock devuelve un monto erróneo */
      if( (j = currentCredits2cents(pMonto)) )
        return j;
    }
    else
    {
      j = AFT_lock_status_request( AFT_STATUS_REQUEST, 0, pMonto );
      if( j == AFT_NO_RESPONSE || j == AFT_BAD_RESPONSE )
      {
        /* Falló la obtención del monto mediante consulta con LP74 --> intenta con LP1A.
           Si también falla, retorna el código de error del LP74. */
        if( currentCredits2cents(pMonto) )
          return j;
      }
    }
  }
  else
  {
    if( (cash_flag /*|| impuesto_afip*/) && !flag.cashoutButtonPressed )
    {
      // Descarga por retiro de tarjeta --> intenta bloquear la máquina
      do
      {
        j = AFT_lock_status_request( AFT_LOCK_REQUEST, AFT_LOCK_4_CASHOUT, pMonto );

        // Envia un 0x80 para mantener sincronizada a la VGM
        delay( pollingPeriod );
        UART0_tx_80();
        delay( pollingPeriod );
      }
      while( *pMonto && ( ++i < 10 ) && !( j==AFT_LOCK_PENDING || j==AFT_LOCKED ) );

      // Espera el evento 6F (EVT_GAME_LOCKED)
      for( wait6FTimer=0, wait6Ftime=_3seg ; ; )
      {
        j = getEvento( &evt );

        delay( pollingPeriod );

        if( j )
        {
          if( filtrarEvento( &evt ) && bufEventos.nEventos < MAX_EVENTOS )
          {
            // Eventos que se reportan al servidor
            bufEventos.evento[bufEventos.nEventos++] = evt;

            // Hace un backup del evento
            //backupEvt(evt);
          }
          UART0_tx_80();  // Hace el ACK implícito del evento a la VGM

          if( evt == EVT_12_MAIN_DOOR_CLOSED )
            flag.doorOpen = 0;
          else if( evt == EVT_6F_GAME_LOCKED )
            break;
          else if( evt == EVT_51_HANDPAY_PENDING )
          {
            // No concreta la descarga, sino que sale para que
            // en el próximo LP se vaya a resetear el pago manual
            flag.handpayRequest = 1;
            return AFT_TRANSFER_CANCELLED_BY_HOST;
          }
        }
        else
        {
          UART0_tx_80();  // Hace el ACK implícito del evento a la VGM
        }
        if( wait6Ftime && wait6FTimer > wait6Ftime )
          return AFT_LOCK_ERROR;  // No pudo obtener el bloqueo de AFT
      }
      delay(pollingPeriod);

      if( !(features.cashless & AFT_CASHOUT_LP1A) )
      {
        // Hace una consulta de bloqueo para obtener el monto a descargar (esta consulta
        // se agregó porque algunas máquinas no entregan el monto actualizado
        // en la respuesta al pedido de bloqueo).
        // Obs.: la única respuesta válida es que la VGM está bloqueada. En caso contrario
        // se retorna con código de error

        if( AFT_lock_status_request( AFT_STATUS_REQUEST, AFT_LOCK_4_CASHOUT, pMonto ) != AFT_LOCKED )
          return AFT_LOCK_ERROR;  // Por las dudas; no debería salir nunca por acá
      }
      else
      {
        /* Consulta el monto a descargar mediante LP1A. Esto es al menos para las ruletas Electrochance,
           que informan un monto erróneo con la consulta de bloqueo. */
        if( (j = currentCredits2cents(pMonto)) )
          return j;
      }
    }
    else if( (cash_flag == DO_CASHOUT /*|| impuesto_afip*/) && flag.cashoutButtonPressed )
    {
      if( !(features.cashless & AFT_CASHOUT_LP1A) )
      {
        // Descarga por botón de pago con evento 6A --> no intenta bloquear la VGM
        // sino que hace una consulta para obtener el monto a descargar
        // Obs.: el manual de SAS dice que en respuesta a un evento 6A se debe
        // hacer un LP 72, por eso no se hace el LP 74 para bloquear.
        j = AFT_lock_status_request( AFT_STATUS_REQUEST, AFT_LOCK_4_CASHOUT, pMonto );

        if( !( j==AFT_LOCKED || j==AFT_LOCK_PENDING || j==AFT_NOT_LOCKED ) )
          return AFT_LOCK_ERROR;  // No pudo efectuar la consulta del bloqueo de AFT
      }
      else
      {
        /* Consulta el monto a descargar mediante LP1A. Esto es al menos para las ruletas Electrochance,
           que informan un monto erróneo con la consulta de bloqueo. */
        if( (j = currentCredits2cents(pMonto)) )
          return j;
      }
    }
    else
    {
      // Situación incompatible (no debería salir nunca por acá)
      return AFT_UNEXPECTED_ERROR;
    }
  }

  delay( pollingPeriod );

  // Si la VGM está en cero, sale como si hubiera descargado
  if( ! *pMonto )
    return AFT_FULL_TRANSFER_OK;

  /* Guarda el monto a descargar que obtuvo del pedido de bloqueo, para compararlo
     con el monto que viene en la respuesta a la autenticación. Si por algún error
     ambos montos son distintos, se descartan los montos y se envía una descarga
     en cero. */
  lock_cashout_monto = *pMonto;

  // Obtiene el transactionID a usar
  AFT_getTransactionID( 0 );

  delay( pollingPeriod );
  UART0_tx_80();
  delay( pollingPeriod );

  // Prepara el AFT init ...
  AFT_prepare_init( AFT_FULL_TRANSFER_REQ,
                    AFT_CASHOUT,
                    AFT_HOST_CASHOUT_ENABLE_CTL|AFT_HOST_CASHOUT_ENABLE|AFT_HOST_CASHOUT_MODE_HARD,
                    *pMonto );

  // ... y lo envía por la UART0
  for(j=0; j<3; ++j)
  {
    UART0_tx( tx_buffer, tx_buflen );

    *pMonto = 0;

    // Espera respuesta y verifica que la respuesta sea válida
    if( !UART0_rx() )
      aftResponse = AFT_NO_RESPONSE;             // La VGM no respondió
    else if( !valid_AFT_response( 0x72 ) )       // La respuesta es incoherente
      aftResponse = AFT_BAD_RESPONSE;
    else
    {
      if(j>=2)
        aftResponse = AFT_NO_RESPONSE;  /* La respuesta es válida pero vino al 3er intento,
                                           por lo que la da por inválida */
      else
        aftResponse = pr->resp.transferStatus;     // Respuesta correcta
      break;
    }
  }

  // En función de la respuesta determina cuanto tiempo debe esperar el evento 69
  // En caso de que la VGM haya respondido con AFT_FULL_TRANSFER_OK no espera el evento 69
  // ni hace la interrogación final
  if( aftResponse != AFT_FULL_TRANSFER_OK )
  {
    if( aftResponse == AFT_TRANSFER_PENDING )
      wait69time = 0;         // Espera indefinidamente por el evento 69
    else
      wait69time = _3seg;     // La VGM no respondió o respondió erróneamente o la respuesta no
    // es la esperada. Por las dudas espera durante 3 segundos por el
    // evento 69, antes de dar por fallada la transacción.

    // Espera el evento 0x69 (AFT TRANSFER COMPLETE)
    for( wait69Timer = 0 ; ; )
    {
      delay( pollingPeriod );
      UART0_tx_80();
      delay( pollingPeriod );

      if( getEvento( &evt ) )
      {
        if( filtrarEvento( &evt ) && bufEventos.nEventos < MAX_EVENTOS )
        {
          // Eventos que se reportan al servidor
          bufEventos.evento[bufEventos.nEventos++] = evt;

          // Hace un backupp del evento
          //backupEvt(evt);
        }
        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM

        if( evt == EVT_12_MAIN_DOOR_CLOSED )
          flag.doorOpen = 0;

        if( evt == EVT_69_AFT_TRANSFER_COMPLETE )
          break;
      }
      else
      {
        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM
      }
      if( wait69time && wait69Timer > wait69time )
        return aftResponse;
    }

    // Va a enviar la interrogación para terminar la transacción AFT
    do
    {
      delay( pollingPeriod );
      UART0_tx_80();
      delay( pollingPeriod );

      // Envía la interrogación con 0xFF para completar la transferencia
      AFT_interrogate();
    }
    while( !UART0_rx() || !valid_AFT_response( 0x72 ) || pr->resp.transferStatus==AFT_TRANSFER_PENDING );
  }

  if( pr->resp.transferStatus == AFT_FULL_TRANSFER_OK )
  {
    // Obtiene el monto del crédito descargado, que reporta la respuesta a la interrogación
    *pMonto = bcd2long( pr->resp.cashableAmount,10 );
    if( *pMonto != lock_cashout_monto )
    {
      /* Si el monto que informa la respuesta a la interrogación no coincide con el monto
         solicitado para la descarga, obtenido de la respuesta al bloqueo, hace una descarga
         en cero, porque no se tiene certeza de que la transacción haya sido correcta */
      *pMonto = 0;
    }
  }
  else
    *pMonto = 0;

  // Retorna el transfer status de la última interrogación (o bien
  // el de la respuesta al init si la respuesta fue AFT_FULL_TRANSFER_OK
  return pr->resp.transferStatus;
}
#if 0
{
  unsigned int i, j;
  uint8_t evt;

  i = j = *pMonto = 0;

  if ( cash_flag && !flag.cashoutButtonPressed )
  {
    // Descarga por retiro de tarjeta --> intenta bloquear la máquina
    do
    {
      j = AFT_lock_status_request ( AFT_LOCK_REQUEST, AFT_LOCK_4_CASHOUT, pMonto);

      delay ( pollingPeriod );
      UART0_tx (0x80, 1);
      delay ( pollingPeriod );
    }
    while ((*pMonto) && (++i < 10) && !(j == AFT_LOCK_PENDING || j == AFT_LOCKED));

    // Espera el evento 6F (EVT_GAME_LOCKED)
    for (wait6FTimer = 0, wait6Ftime = _3seg;;)
    {
      j = getEvento (&evt);

      delay ( pollingPeriod );

      if ( j )
      {
        if ( filtrarEvento (&evt) && bufEventos.nEventos < MAX_EVENTOS )
        {
          // Eventos que se reportan al servidor
          bufEventos.evento[bufEventos.nEventos++] = evt;

          //          // Hace un backupp del evento
          //          backupEvt(evt);
        }
        UART0_tx (0x80, 1);  // Hace el ACK implícito del evento a la VGM

        if ( evt == EVT_12_MAIN_DOOR_CLOSED )
          flag.doorOpen = 0;
        else if ( evt == EVT_6F_GAME_LOCKED )
          break;
        else if ( evt == EVT_51_HANDPAY_PENDING )
        {
          // No concreta la descarga, sino que sale para que
          // en el próximo LP se vaya a resetear el pago manual
          flag.handpayRequest = 1;
          return AFT_TRANSFER_CANCELLED_BY_HOST;
        }
      }
      else
      {
        UART0_tx (0x80, 1);  // Hace el ACK implícito del evento a la VGM
      }
      if ( wait6Ftime && wait6FTimer > wait6Ftime )
        return AFT_LOCK_ERROR;  // No pudo obtener el bloqueo de AFT
    }
    delay ( pollingPeriod );

    // Hace una consulta de bloqueo para obtener el monto a descargar (esta consulta
    // se agregó porque algunas máquinas no entregan el monto actualizado
    // en la respuesta al pedido de bloqueo).
    // Obs.: la única respuesta válida es que la VGM está bloqueada. En caso contrario
    // se retorna con código de error

    if ( AFT_lock_status_request ( AFT_STATUS_REQUEST, AFT_LOCK_4_CASHOUT, pMonto) != AFT_LOCKED )
      return AFT_LOCK_ERROR;  // Por las dudas; no debería salir nunca por acá

  }
  else if ( cash_flag == DO_CASHOUT && flag.cashoutButtonPressed )
  {
    // Descarga por botón de pago con evento 6A --> no intenta bloquear la VGM
    // sino que hace una consulta para obtener el monto a descargar
    // Obs.: el manual de SAS dice que en respuesta a un evento 6A se debe
    // hacer un LP 72, por eso no se hace el LP 74 para bloquear.
    j = AFT_lock_status_request ( AFT_STATUS_REQUEST, AFT_LOCK_4_CASHOUT, pMonto);

    if ( !(j == AFT_LOCKED || j == AFT_LOCK_PENDING || j == AFT_NOT_LOCKED) )
      return AFT_LOCK_ERROR;  // No pudo efectuar la consulta del bloqueo de AFT
  }
  else
  {
    // Situación incompatible (no debería salir nunca por acá)
    return AFT_UNEXPECTED_ERROR;
  }

  delay ( pollingPeriod );

  // Si la VGM está en cero, sale como si hubiera descargado
  if ( !(*pMonto) )
    return AFT_FULL_TRANSFER_OK;

  // Obtiene el transactionID a usar
  AFT_getTransactionID (0);

  delay ( pollingPeriod );
  UART0_tx (0x80, 1);
  delay ( pollingPeriod );

  // Prepara el AFT init
  AFT_prepare_init ( AFT_FULL_TRANSFER_REQ,
  AFT_CASHOUT,
                    AFT_HOST_CASHOUT_ENABLE_CTL | AFT_HOST_CASHOUT_ENABLE | AFT_HOST_CASHOUT_MODE_HARD, *pMonto);

  // y lo envía mensaje por la UART0
  UART0_tx (tx_buffer[0], 1);               // primer byte --> dirección de la VGM - wakeup bit = 1

  for (i = 1; i < tx_buflen; ++i)
    UART0_tx (tx_buffer[i], 0);             // resto del mensaje -> wakeup bit = 0

  *pMonto = 0;

  // Espera respuesta
  if ( !UART0_rx (sizeof(AFT72Type)) )
  {
    aftResponse = AFT_NO_RESPONSE;            // La VGM no respondió
  }
  else if ( !valid_AFT_response (0x72) )       // Verifica que la respuesta sea válida
  {
    // e indique transferencia pendiente
    aftResponse = AFT_BAD_RESPONSE;
  }
  else
  {
    aftResponse = pr->resp.transferStatus;
  }

  // En función de la respuesta determina cuanto tiempo debe esperar el evento 69
  // En caso de que la VGM haya respondido con AFT_FULL_TRANSFER_OK no espera el evento 69
  // ni hace la interrogación final
  if ( aftResponse != AFT_FULL_TRANSFER_OK )
  {
    if ( aftResponse == AFT_TRANSFER_PENDING )
      wait69time = 0;         // Espera indefinidamente por el evento 69
    else
      wait69time = _3seg;     // La VGM no respondió o respondió erróneamente o la respuesta no
    // es la esperada. Por las dudas espera durante 3 segundos por el
    // evento 69, antes de dar por fallada la transacción.

    // Espera el evento 0x69 (AFT TRANSFER COMPLETE)
    for (wait69Timer = 0;;)
    {
      delay ( pollingPeriod );
      UART0_tx (0x80, 1);
      delay ( pollingPeriod );

      if ( getEvento (&evt) )
      {
        if ( filtrarEvento (&evt) && bufEventos.nEventos < MAX_EVENTOS )
        {
          // Eventos que se reportan al servidor
          bufEventos.evento[bufEventos.nEventos++] = evt;

          //          // Hace un backup del evento
          //          backupEvt(evt);
        }
        UART0_tx (0x80, 1);  // Hace el ACK implícito del evento a la VGM

        if ( evt == EVT_12_MAIN_DOOR_CLOSED )
          flag.doorOpen = 0;

        if ( evt == EVT_69_AFT_TRANSFER_COMPLETE )
          break;
      }
      else
      {
        UART0_tx (0x80, 1);  // Hace el ACK implícito del evento a la VGM
      }
      if ( wait69time && wait69Timer > wait69time )
        return aftResponse;
    }

    // Va a enviar la interrogación para terminar la transacción AFT
    do
    {
      delay ( pollingPeriod );
      UART0_tx (0x80, 1);
      delay ( pollingPeriod );

      // Envía la interrogación con 0xFF para completar la transferencia
      AFT_interrogate ();

    }
    while (!UART0_rx (sizeof(AFT72Type)) || !valid_AFT_response (0x72) || pr->resp.transferStatus == AFT_TRANSFER_PENDING);
  }

  if ( pr->resp.transferStatus == AFT_FULL_TRANSFER_OK )
    // Obtiene el monto del crédito descargado
    *pMonto = bcd2long ( pr->resp.cashableAmount, 10);
  else
    *pMonto = 0;

  // Retorna el transfer status de la última interrogación (o bien
  // el de la respuesta al init si la respuesta fue AFT_FULL_TRANSFER_OK
  return pr->resp.transferStatus;
}
#endif

#undef wait69time
#undef wait6Ftime
#undef aftResponse
#undef pr

/*
 * Transacción AFT para descargar un premio de la VGM
 * Retorna: 1 (AFT_PARTIAL_TRANSFER_OK)
 *          o el Transfer Status que indica por que no se completó la transferencia
 *          o 0x0F (la VGM no responde)
 *          o 0x2F (Error en AFT Lock)
 *          o 0x1F (otra causa de error)
 *          La variable cash_monto queda cargada con el monto descargado
 */
#define pr ((AFT72Type *)rx_buffer)
#define wait69time i
#define wait6Ftime i
#define aftResponse j

static uint8_t AFT_cashout_win( uint32_t * pMonto )
{
  uint32_t i, j;
  uint8_t evt;

  i = j = 0;
  *pMonto = (unsigned long)9999999999;

  // Obtiene el transactionID a usar
  AFT_getTransactionID( 0 );

  delay( pollingPeriod );
  UART0_tx_80();
  delay( pollingPeriod );

  // Prepara el AFT init ...
  AFT_prepare_init( AFT_PARTIAL_TRANSFER_REQ,
                    AFT_CASHOUT_WIN,
                    AFT_HOST_CASHOUT_ENABLE_CTL|AFT_HOST_CASHOUT_ENABLE|AFT_HOST_CASHOUT_MODE_HARD,
                    *pMonto );

  // ... y lo envía por la UART0
  for(j=0; j<3; ++j)
  {
    UART0_tx( tx_buffer, tx_buflen);

    *pMonto = 0;

    // Espera respuesta y verifica que la respuesta sea válida
    if( !UART0_rx() )
      aftResponse = AFT_NO_RESPONSE;             // La VGM no respondió
    else if( !valid_AFT_response( 0x72 ) )       // La respuesta es incoherente
      aftResponse = AFT_BAD_RESPONSE;
    else
    {
      if(j>=2)
        aftResponse = AFT_NO_RESPONSE;  /* La respuesta es válida pero vino al 3er intento,
                                           por lo que la da por inválida */
      else
        aftResponse = pr->resp.transferStatus;     // Respuesta correcta
      break;
    }
  }

  // En función de la respuesta determina cuanto tiempo debe esperar el evento 69
  // En caso de que la VGM haya respondido con AFT_FULL_TRANSFER_OK no espera el evento 69
  // ni hace la interrogación final
  if( aftResponse != AFT_PARTIAL_TRANSFER_OK )
  {
    if( aftResponse == AFT_TRANSFER_PENDING )
      wait69time = 0;         // Espera indefinidamente por el evento 69
    else
      wait69time = _3seg;     // La VGM no respondió o respondió erróneamente o la respuesta no
    // es la esperada. Por las dudas espera durante 3 segundos por el
    // evento 69, antes de dar por fallada la transacción.

    // Espera el evento 0x69 (AFT TRANSFER COMPLETE)
    for( wait69Timer = 0 ; ; )
    {
      delay( pollingPeriod );
      UART0_tx_80();
      delay( pollingPeriod );

      if( getEvento( &evt ) )
      {
        if( filtrarEvento( &evt ) && bufEventos.nEventos < MAX_EVENTOS )
        {
          // Eventos que se reportan al servidor
          bufEventos.evento[bufEventos.nEventos++] = evt;

          // Hace un backup del evento
          //backupEvt(evt);
        }
        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM

        if( evt == EVT_12_MAIN_DOOR_CLOSED )
          flag.doorOpen = 0;

        if( evt == EVT_69_AFT_TRANSFER_COMPLETE )
          break;
      }
      else
      {
        UART0_tx_80();  // Hace el ACK implícito del evento a la VGM
      }
      if( wait69time && wait69Timer > wait69time )
        return aftResponse;
    }

    // Va a enviar la interrogación para terminar la transacción AFT
    do
    {
      delay( pollingPeriod );
      UART0_tx_80();
      delay( pollingPeriod );

      // Envía la interrogación con 0xFF para completar la transferencia
      AFT_interrogate();

    }
    while( !UART0_rx() || !valid_AFT_response( 0x72 ) || pr->resp.transferStatus==AFT_TRANSFER_PENDING );
  }

  if( pr->resp.transferStatus == AFT_PARTIAL_TRANSFER_OK )
    // Obtiene el monto del crédito descargado
    *pMonto = bcd2long( pr->resp.cashableAmount,10 );
  else
    *pMonto = 0;

  // Retorna el transfer status de la última interrogación (o bien
  // el de la respuesta al init si la respuesta fue AFT_FULL_TRANSFER_OK
  return pr->resp.transferStatus;
}

#undef wait69time
#undef wait6Ftime
#undef aftResponse
#undef pr

/**
 * @brief Resetea un pago manual pendiente y transfiere el monto al servidor.
 * @param evt Indica que evento originó el llamado a la función
 *            evt == 0x51 (EVT_HANDPAY_PENDING) --> intentar el reset
 *            evt == 0x52 (EVT_HANDPAY_RESET) --> si fue reseteado con
 *                   la llave, transfiere el monto al servidor
 * @return 1: OK / 0: Error
 */

#define pi ((HandpayInfoType *)rx_buffer)
#define pr ((ResetHandpayResponseType *)rx_buffer)

uint8_t reset_handpay( uint8_t evt )
{
  int i, j;
  static uint32_t monto, monto51 = 0, t0;
  static uint8_t group=0, level=0;

  if( evt == EVT_51_HANDPAY_PENDING )
  {
    // Consulta estado de handpay
    for( j=0; j<3; ++j )
    {
      UART0_tx_lpr(0x1B);

      if( UART0_rx() && rx_buflen==sizeof(HandpayInfoType) && !CRC16( rx_buffer,rx_buflen,0 ) && pr->vgmAddr==vgmAddr && pr->command==0x1B )
        break;
    }

    if( j > 2 )
      return 0;     // No hubo respuesta al LP 1B --> aborta

    // Envía 0x80 y espera durante el período entre pollings
    delay( pollingPeriod );
    UART0_tx_80();
    delay( pollingPeriod );

    // Obtiene el monto a descargar y prepara el mensaje de cashout

    // Obs.: como puede suceder que después de haber obtenido el monto a descargar
    // se haya abierto la puerta de la VGM (u otro evento que bloquea el pago manual)
    // y no se haya podido concretar el reset, se debe guardar dicho monto dado que
    // un pedido de info posterior da cero.
    // Por eso, si un pedido de info de pago manual da monto cero, se utiliza el
    // valor que contenga de antemano la variable "monto51".

    if( ( monto = bcd2long( pi->amount, 10 ) ) )
    {
      monto51 = monto;
      group = pi->progGroup;
      level = pi->progLevel;
    }

    if( monto51 && (features.cashless & CASHLESS_ENABLED) && !cfg.eibe )
    {
      // Envía mensaje a la VGM para resetear el pago manual.
      // En el caso de las EIBE no lo hace, para que tenga que hacerlo manualmente un operador
      // y registrar el monto, a fin de evitar errores que estarían dando estas máquinas.
      tx_buflen = 4;
      tx_buffer[0] = vgmAddr;
      tx_buffer[1] = 0x94;
      i = CRC16( tx_buffer, tx_buflen-2, 0 );
      tx_buffer[2] = i & 0x00ff;
      tx_buffer[3] = ( i >> 8 ) & 0x00ff;

      // Si no recibe respuesta al reset handpay, intenta el envío hasta 3 veces
      for( j=0; j<3; ++j )
      {
        UART0_tx( tx_buffer, tx_buflen);

        if(UART0_rx() && rx_buflen==sizeof(ResetHandpayResponseType) && !CRC16(rx_buffer,rx_buflen,0) && pr->vgmAddr==vgmAddr && pr->command==0x94)
          break;
      }

      // Envía 0x80 y espera durante el período entre pollings
      delay( pollingPeriod );
      UART0_tx_80();
      delay( pollingPeriod );

      if( j > 2 )
      {
        if( monto51 )
          return 0;     /* No hubo respuesta al reset y todavía hay un monto a descargar -->
                           aborta para volver a intentar o esperar a que se resetee con la llave */
      }
      else
      {
        if( pr->resetCode == 1 )
          return 0;     // No pudo resetear --> aborta para volver a intentar en la próxima vuelta
      }
    }
  }
  else if( evt != EVT_52_HANDPAY_RESET )
  {
    // Opción inválida: sale sin hacer nada
    return 0;
  }

  //-------------------------------------------------------
  // Si hay un monto de pago manual, lo envía al servidor
  //-------------------------------------------------------

  if( monto51 && !waitingAnyServerReply() )
  {
    // Prepara el mensaje que enviará al servidor

    // Obtiene los contadores actualizados de coin in, coin out, jackpot
    // y total hand paid cancelled credits, que deben ser incluidos en el
    // mensaje de handpay

    delay( pollingPeriod );
    polling(NPOLL_COIN_IN);
    delay( pollingPeriod );
    polling(NPOLL_TOTAL_HANDPAID_CANCELLED_CREDITS);
    //----------------------------

    fillZeros((unsigned char *)&cashMsgBuffer, sizeof(cashMsgBuffer));  // Inicializa con ceros el buffer de mensaje

    prepareHandpayMsgBuffer();  /* Carga los campos básicos del mensaje, que son comunes
                                   a los mensajes de reporte de pago de progresivo
                                   y legacy bonus */

    cashMsgBuffer.errorCode = level;

    cashMsgBuffer.evt = EVT_51_HANDPAY_PENDING;
    cashMsgBuffer.progGroup = group;
    cashMsgBuffer.progLevel = level;

    sprintf( ( char * )cashMsgBuffer.monto, "%010lu", monto51 );

    prepareCashMsgBuffer(); // Carga mac, uid, hash y crc. Luego cifra el mensaje

    while(1)
    {
      // Envía mensaje al servidor de online, para que registre el monto del pago manual
      flag.waitingHandpayReply = 1;

      // resetea la variable que contiene el monto descargado
      monto51 = 0;

      // resetea el flag de requerimiento de reset de pago manual
      flag.handpayRequest = 0;

      // Si el pago manual provenía del algún progresivo, ordena enviar
      // el mensaje de progressive win para computar ese premio
      if(level>0 && level<=20)
        flag.progressiveWin = 1;

      sendUDP( ( unsigned char * )&cashMsgBuffer, sizeof( cashMsgBuffer ), upcb_cli, &addr_serv_online, SERV_PORT_PAGO_MANUAL );

      // Espera recibir la respuesta del servidor de online
      t0 = HAL_GetTick();
      while( flag.waitingHandpayReply && (HAL_GetTick()-t0)<_3seg );

      if ( !flag.waitingHandpayReply )
      {
        flag.waitingCashoutReply = 1;

        decifrar( ( uint8_t * )&cashMsgBuffer, sizeof( cashMsgBuffer ) );

        if( ( features.cashless & CASHLESS_ENABLED ) && !cfg.eibe && cashMsgBuffer.evt==EVT_51_HANDPAY_PENDING )
        {
          // Envía mensaje de pago manual al servidor de cashless, para que acredite el monto descargado.
          // En el caso de las EIBE no lo hace, porque hay dudas de que el pago manual y su monto sean
          // válidos. En todo caso, ante un reclamo del jugador se le acreditará el monto registrado por
          // el asistente que reseteó el pago manual.

          cifrar( ( uint8_t * )&cashMsgBuffer, sizeof( cashMsgBuffer ) );

          while(1)
          {
            sendUDP( ( unsigned char * )&cashMsgBuffer, sizeof( cashMsgBuffer ), upcb_cli, &addr_serv_cashless, SERV_PORT_CASHLESS );

            t0 = HAL_GetTick();
            while( flag.waitingCashoutReply && (HAL_GetTick()-t0)<_3seg );

            if ( !flag.waitingCashoutReply )
              break;
          }
          break;
        }
      }
    }
  }
  // por las dudas resetea el monto acumulado del último vendedor
  montoVendedor = 0;
  //montoVendedorPesos = 0;

  /* Resetea el flag de espera del evento 52, que mantenía bloqueada la posibilidad
     de hacer un cashout (eventualmente en cero) por retiro de tarjeta. Esto es para
     impedir que la descarga en cero por retiro de tarjeta llegue antes que el pago
     manual, haciendo que este último se pierda. */
  flag.waitingEvent52 = 0;

  return 1;
}

#undef pi
#undef pr

/*
 * Carga crédito por Legacy Bonus
 * Retorna:
 *           0: LEGACY_BONUS_OK
 *           1: LEGACY_BONUS_REJECT (carga rechazada)
 *           2: LEGACY_BONUS_VGM_BUSY (VGM ocupada --> intentar más tarde)
 *        0xE1: LEGACY_BONUS_NO_RESPONSE (La VGM no responde)
 *        0xE2: LEGACY_BONUS_BAD_RESPONSE (La VGM respondió de manera inconsistente)
 */
#define pt ((InitLegacyBonusCommandType *)tx_buffer)

uint8_t LegacyBonus()
{
  uint32_t i, j;

  tx_buflen = sizeof( InitLegacyBonusCommandType );

  // Prepara el envío del mensaje

  pt->vgmAddr = vgmAddr;
  pt->command = 0x8A;
  long2bcd( pt->amount, montoLegacyBonus, 8 );
  pt->tax_status = 0;
  pt->crc = CRC16( tx_buffer, tx_buflen-2, 0 );

  for(j=0; j<2; ++j)
  {
    UART0_tx(tx_buffer, tx_buflen);

    // Espera la respuesta
    UART0_rx();

    if(rx_buflen)
    {
      // Si la VGM respondió sale del bucle, si no, reintenta hasta dos veces.
      // Obs.: no reintenta tres veces porque a la tercera no responde e igualmente
      //       da por válida la carga. Es decir, si se respeta el protocolo de ACK
      //       implícito del SAS y se repite el comando tres veces, a la tercera no
      //       responde, pero tampoco deshace la carga de créditos.
      break;
    }
  }
  // Prepara el mensaje a enviar al servidor
  cashMsgBuffer.msgCode = CASHOUT_REQ;

  switch( rx_buflen )
  {
  case 0:
    cashMsgBuffer.msgCode = LEGACY_BONUS_NAK;
    cashMsgBuffer.errorCode = LEGACY_BONUS_NO_RESPONSE;
    break;
  case 1:
    if( rx_buffer[0]==vgmAddr )
    {
      cashMsgBuffer.msgCode = LEGACY_BONUS_ACK;      // OK
      cashMsgBuffer.errorCode = LEGACY_BONUS_OK;
    }
    else if( rx_buffer[0]==( vgmAddr|0x80 ) )
    {
      cashMsgBuffer.msgCode = LEGACY_BONUS_NAK;      // ERROR
      cashMsgBuffer.errorCode = LEGACY_BONUS_REJECT; // La VGM rechazó el comando
    }
    else
    {
      cashMsgBuffer.msgCode = LEGACY_BONUS_NAK;            // ERROR
      cashMsgBuffer.errorCode = LEGACY_BONUS_BAD_RESPONSE; // Respuesta inconsistente de la VGM
    }
    break;
  case 2:
    cashMsgBuffer.msgCode = LEGACY_BONUS_NAK;          // ERROR
    if( rx_buffer[0]==vgmAddr && rx_buffer[1]==0x00 )
      cashMsgBuffer.errorCode = LEGACY_BONUS_VGM_BUSY; // La VGM está ocupada y no va a procesar el comando
    else
      cashMsgBuffer.errorCode = LEGACY_BONUS_BAD_RESPONSE; // Respuesta inconsistente de la VGM
    break;
  default:
    cashMsgBuffer.msgCode = LEGACY_BONUS_NAK;              // ERROR
    cashMsgBuffer.errorCode = LEGACY_BONUS_BAD_RESPONSE;   // Respuesta inconsistente de la VGM
  }

  for( i=0; i<3; ++i )
    cashMsgBuffer.mac[i] = MyMAC[i];
  cashMsgBuffer.uid = uid;
  cashMsgBuffer.hash = rand();
  cashMsgBuffer.crc = CRC16( ( uint8_t * )&cashMsgBuffer,sizeof( cashMsgBuffer )-2,0 );
  cifrar( ( uint8_t * )&cashMsgBuffer,sizeof( cashMsgBuffer ) );

  // Envía respuesta al controlador de progresivo, con el resultado de la operación
  sendUDP( ( uint8_t * )&cashMsgBuffer, sizeof( cashMsgBuffer ), upcb_bonus, &addr_serv_prog, SERV_PORT_PROGRESIVO );

  return cashMsgBuffer.errorCode;
}

#undef pt
