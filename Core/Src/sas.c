/*
 * sas.c
 *
 *  Created on: Oct 2, 2026
 *      Author: guillermo
 */

#include "cashless.h"

volatile union FLAGS flag, flagsWaitingServerReply;

// constantes para identificar el tipo de poll a realizar

enum
{
  POLL_SINC1,         // Solo envia el 0x80 para sincronizar
  POLL_GP,
  POLL_SINC2,         // Solo envia el 0x80 para sincronizar
  POLL_LP,
  POLL_CASH,
  POLL_SINC3,         // Solo envia el 0x80 para sincronizar
  POLL_LP1A,          // Consulta de créditos posterior a una operación cashless
  POLL_SINC4,         // Solo envia el 0x80 para sincronizar
  POLL_LP1C,          // Consulta coin in, coin out, jackpot, games won y games played, previo a un cashout
  POLL_TICKET,        // Procesa TITO
  POLL_HANDPAY_52,    // Procesa un reset de pago manual
  POLL_SINC_LP1C_PROG,// Solo envia el 0x80 para sincronizar (antes del LP para los progresivos)
  POLL_LP1C_PROG,     // Hace un LP1C adicional para actualizar el valor de coin in para el progresivo
  POLL_PROG_BONUS_WIN // Informar datos del último premio progresivo o legacy bonus al servidor
};

/* ── UART / DMA ── */
#define UART_DMA_BUF_SIZE   256
uint8_t  uart_dma_buf[UART_DMA_BUF_SIZE];   /* buffer circular DMA */
uint16_t uart_dma_head = 0;                  /* última posición procesada */

#if 0
typedef struct {
    uint32_t start;      /* valor de HAL_GetTick() al arrancar */
    uint32_t interval;   /* intervalo en ms */
    uint8_t  running;    /* 1 = activo */
} SoftTimer_t;

SoftTimer_t timer_led_blink;   /* ejemplo: parpadeo LED en error */
SoftTimer_t timer_sync;        /* sync periódico a pendrive */
SoftTimer_t timer_tx;          /* Inicio de transmisión */
SoftTimer_t timer_sas_polling; /* Periodicidad del polling SAS */

uint32_t usb_connect_timeout = 0;
#endif

// Contador de eventos no respondidos (usado para determinar si la VGM está desconectada)
static int lostEvents;

// Nro de secuencia del mensaje de eventos, usado para diferenciar mensajes consecutivos
static uint8_t nSecEventMsg = 0;

//Si es distinto de cero indica que el mensaje de eventos es una retransmisión
static uint32_t eventMsgRetry = 0;

char vgmID[3];  // Código de 2 char que identifica al fabricante. Por ej.: "EE" --> EIBE (ver tabla C-1 del manual del SAS)

uint32_t nRetry2LockVGM; // Nro de intentos de envío de Hello antes de bloquear la VGM

uint32_t uid;       // UID de la VGM, según el padrón de la sala

uint8_t MyMAC[6];       // Dirección MAC

uint32_t currentCredits;       // último valor obtenido con el LP 1A

uint8_t vgmAddr;       /* Dirección de la máquina (para EIBE usa 0x01,
                          para SAS detecta la dirección que tenga) */

uint32_t pollingPeriod = _40ms;  // Periodo entre envío de pollings a la máquina

uint32_t montoLegacyBonus;       // Monto a cargar por Legacy Bonus

volatile union CONFIG_VGM cfg;

uint32_t unidad_denominacion = 0;  // denominación en centavos según código en LP1F

EventMsgType bufEventos;           // Buffer del mensaje de eventos

static int nextPoll = POLL_SINC1;

// Estructura de campos de bits que contiene
// los flags que controlan las operaciones de tickets
volatile TICKET_FLAGS ticket_flag;

/* Variable usada para ordenar una operación de cashless, como consecuencia de una acción
   del jugador: ingresar tarjeta, retirar tarjeta, presionar botón de cashout, etc. */
int cash_flag;

// Estructura del mensaje usado para obtener información del último premio del progresivo
typedef struct __attribute__((packed))
{
  uint8_t vgmAddr;
  uint8_t command;         // 0x1B
  uint8_t progGroup;
  uint8_t progLevel;
  uint8_t amount[5];
  uint8_t crc[2];
} LastProgressiveWinType;

// Estructura del mensaje usado para obtener información de un legacy bonus pagado
typedef struct __attribute__((packed))
{
  uint8_t vgmAddr;
  uint8_t command;           // 0x90
  uint8_t multiplier;
  uint8_t multiplied_win[4]; // monto de premio multiplicado, en créditos (8 BCD)
  uint8_t tax_status;        // 0x00: deducible - 0x01: no deducible - 0x02: wager watch
  uint8_t bonus_amount[4];   // monto del bonus, en créditos (8 BCD)
  uint16_t crc;
} LegacyBonusWinInfoType;

// Prototipos de funciones static

static void initVars( void );
static void initSAS( void );
static uint8_t disableRealTimeEventReport( void );
static uint8_t sendInfoProgBonus (uint8_t evt);
static void SAS_processing(void);
static void clearEventBuffer(void);
static int general_poll (void);
static int poll_cash (void);
static void mensaje_al_bar( uint8_t evt );

#if 0
/* ─────────────────────────────────────────────
 * LED helpers
 * ───────────────────────────────────────────── */
static void leds_all_off(void)
{
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);
}

static void led_set(uint16_t pin, GPIO_PinState state)
{
  HAL_GPIO_WritePin(GPIOD, pin, state);
}

static void led_toggle(uint16_t pin)
{
  HAL_GPIO_WritePin(GPIOD, pin, !HAL_GPIO_ReadPin(GPIOD, pin));
}

/* ─────────────────────────────────────────────
 * Timer TIM6: arrancar / reiniciar cuenta de 40ms
 * ───────────────────────────────────────────── */
static void gap_timer_restart(void)
{
    __HAL_TIM_SET_COUNTER(&htim6, 0);
    HAL_TIM_Base_Start_IT(&htim6);
}

static void gap_timer_stop(void)
{
    HAL_TIM_Base_Stop_IT(&htim6);
}

/* ─────────────────────────────────────────────
 * SoftTimer: arrancar
 * ───────────────────────────────────────────── */
static void stimer_start(SoftTimer_t *t, uint32_t interval_ms)
{
    t->start    = HAL_GetTick();
    t->interval = interval_ms;
    t->running  = 1;
}

/* ─────────────────────────────────────────────
 * SoftTimer: consultar si expiró
 * Retorna 1 una sola vez al expirar, luego
 * se reinicia automáticamente (modo periódico)
 * ───────────────────────────────────────────── */
static uint8_t stimer_expired(SoftTimer_t *t)
{
    if (!t->running) return 0;

    if ((HAL_GetTick() - t->start) >= t->interval)
    {
        t->start = HAL_GetTick();   /* reiniciar para el próximo período */
        return 1;
    }
    return 0;
}

/* ─────────────────────────────────────────────
 * SoftTimer: detener
 * ───────────────────────────────────────────── */
static void stimer_stop(SoftTimer_t *t)
{
    t->running = 0;
}

/* ─────────────────────────────────────────────
 * SoftTimer: consultar tiempo transcurrido
 * ───────────────────────────────────────────── */
static uint32_t stimer_elapsed(SoftTimer_t *t)
{
    if (!t->running) return 0;
    return (HAL_GetTick() - t->start);
}
#endif

void delay(uint32_t mseg)
{
  uint32_t t0 = HAL_GetTick();
  while( (HAL_GetTick() - t0) < mseg );
}

void DoNetworkStuff()
{
  uint32_t t0;
  static uint32_t update=0;

  t0 = HAL_GetTick();
  do
  {
    IWDG_REFRESH();
    MX_LWIP_Process();      /* procesar stack LWIP — incluye eth rx/tx */

    if( flag.updatingFirmware && !update )
      LCD_print("DESCARGANDO","FIRMWARE");
    else if( !flag.updatingFirmware && update )
      LCD_print("DESCARGA FW","CANCELADA");
    update = flag.updatingFirmware;
  }
  while ( flag.updatingFirmware && (HAL_GetTick()-t0)<_30seg );

  flag.updatingFirmware = 0; // Por si hubiera salido por timeout
}

/**
  * @brief  The application entry point.
  * @retval void
  */
void myMain(void)
{
  uint32_t sendMeters_timer = 0;
  //uint32_t flag=0, n;

  Debug_FreezeWatchdog();
  IWDG_Init_Register();

  /* STMPS2141STR: EN activo en bajo — forzar PC0 a 0 para activar VBUS */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_Delay(1000);

  //stimer_start(&timer_tx, 1000);
  //stimer_start(&timer_sas_polling, 200);

  initVars();

  LCD_init();

  clearEventBuffer();
  upcb_cli = udp_server_init(LOCAL_PORT_CLI);
  upcb_mgmt = udp_server_init(LOCAL_PORT_MGMT);
  upcb_lp21 = udp_server_init(LOCAL_PORT_LP21);
  upcb_flash = udp_server_init(LOCAL_PORT_FLASH);

  sendHello("INICIANDO","");
/*
  while(1)
  {
    DoNetworkStuff();
    if( ++flag%3 == 1 )
    {
      UART0_tx("HOLA QUE ",9);
      HAL_UART_Transmit(&huart5, (uint8_t *)"T", 1, HAL_MAX_DELAY);
      HAL_UART_Transmit(&huart5, (uint8_t *)"A", 1, HAL_MAX_DELAY);
      delay(100);
      HAL_UART_Transmit(&huart5, (uint8_t *)"L", 1, HAL_MAX_DELAY);
      HAL_UART_Transmit(&huart5, (uint8_t *)" ", 1, HAL_MAX_DELAY);
      delay(150);
      HAL_UART_Transmit(&huart5, (uint8_t *)"A", 1, HAL_MAX_DELAY);
      HAL_UART_Transmit(&huart5, (uint8_t *)"B", 1, HAL_MAX_DELAY);
    }else if(flag%3 == 2) {
      UART0_tx("BYE BYE ",9);
    }else{
      strcpy(rx_buffer,"XYZ XXYYZZ");
      UART0_tx((uint8_t *)"123", 4);
      delay(500);
    }

    if( n = UART0_rx() )
    {
      rx_buffer[rx_buflen]=0;
      sprintf((char *)tx_buffer+64,"%s %d %u", rx_buffer+15, rx_buflen, n);
      LCD_print(rx_buffer,tx_buffer+64);
      rx_buflen=0;
    }else{
      sprintf((char *)tx_buffer+64,"%d %u", rx_buflen, n);
      LCD_print("",tx_buffer+64);
    }
  }
*/
  initSAS();

  /* Infinite loop */
  while (1)
  {
    DoNetworkStuff();

    Hello_process();        /* Envío periódico del Hello y procesamiento de las respuestas */

    //UDP_Echo_Process();     /* reservado para lógica futura */

    //MX_USB_HOST_Process();  /* ── Procesar USB Host ── */

    SAS_processing();

    if( (HAL_GetTick() - sendMeters_timer) > _30seg )
    {
      sendMeters_timer = HAL_GetTick();
      sendMeters();
    }
  }
}

/**
 * Función de procesamiento del SAS
 */
void SAS_processing()
{
  static uint32_t t0 = 0;
  // para guardar temporalmente el valor de games played, a fin de verificar si
  // tuvo variación, y en ese caso reportar el coin in al servidor de progresivo
  static uint32_t games_played;

  if( (HAL_GetTick() - t0) > pollingPeriod)
  {
    t0 = HAL_GetTick();

    if (flag.payLegacyBonus)
    {
      flag.payLegacyBonus = 0;
      LegacyBonus ();
      return;  // Sale para luego volver a iniciar otro ciclo
    }

    if( flag.onDemandLongPoll )
      // Se recibió un requerimiento de LP a demanda --> lo procesa
      OnDemandLongPoll(0,0,0,0);

    switch (nextPoll)
    {
    case POLL_SINC1:
      // Envia un 0x80 para mantener sincronizada a la VGM
      UART0_tx_80();

      if ( waitingAnyServerReply() || !flag.fechaHoraValida )
      {
        // Está esperando el ACK al mensaje de eventos o una respuesta de cashless o TITO,
        // o bien no tiene fijada una fecha y hora dada por el servidor.
        // Saltea el pedido de eventos y va hacer un long poll
        nextPoll = POLL_LP;
      }
      else
      {
        // La próxima vez hay que hacer un GP
        nextPoll = POLL_GP;
      }
      break;

    case POLL_GP:
      // Obtiene un evento y lo procesa.
      // Dependiendo del evento obtenido el próximo paso podría ser POLL_SINC2,
      // POLL_HANDPAY_52 o POLL_SINC1
      nextPoll = general_poll();
      break;

    case POLL_SINC2:
      // Envía un 0x80 para mantener sincronizada a la VGM
      UART0_tx_80();

      if (flag.waitingEventAck)
      {
        // Como está esperando el ACK a un mensaje de eventos, no verifica si se puso o quitó una
        // tarjeta, porque en caso de tener que hacer una transacción AFT va a tener que leer
        // eventos y eventualmente almacenarlos en el buffer.
        // Por lo tanto, la próxima vez va a hacer un LP
        nextPoll = POLL_LP;
        break;
      }

      if ( !waitingAnyServerReply() && !cash_flag )
      {
        // Verifica si se insertó o se retiró una tarjeta
        checkCardReader ();
      }

      if (ticket_flag.do_set_validation_id)
        // se inició con validation id not set --> lo procesa en primer lugar
        nextPoll = POLL_TICKET;
      else if (cash_flag == DO_CASHOUT)
        /* Se retiró la tarjeta o se ordenó un cashout -->
           --> el próximo paso es ir a hacer la descarga correspondiente. */
        nextPoll = POLL_CASH;
      else if (flag.handpayRequest)
        /* Hay un pago manual pendiente, entonces el próximo paso antes de procesar
           el pago manual es consultar los contadores de LP1C y LP2F, porque
           dichos valores van en el mensaje. */
        nextPoll = POLL_LP1C;
      else if (cash_flag == DO_CASHIN)
        /* Se ingresó una tarjeta, entonces el próximo paso es consultar coin in, coin out, etc.
         y luego hacer cashin().
         Obs.: si está en medio de una carga de vendedor saltea estos pasos, porque ya
         hizo LP1C y LP2F cuando insertó la tarjeta del vendedor, y va directamente
         a hacer cashin(). */
        if (montoVendedor)
          nextPoll = POLL_CASH;
        else
          nextPoll = POLL_LP1C;
      else if (*(unsigned int*) &ticket_flag)
        // se ingresó o se imprimió un ticket --> en el próximo LP va a procesarlo
        nextPoll = POLL_TICKET;
      else if ((flag.progressiveWin || flag.legacyBonusWin) && !waitingAnyServerReply())
        nextPoll = POLL_PROG_BONUS_WIN;
      else
        // No hay requerimientos específicos --> la próxima vez hay que hacer un LP
        nextPoll = POLL_LP;
      break;

    case POLL_PROG_BONUS_WIN:
      // Se pagó un premio progresivo o legacy bonus y lo debe informar al servidor
      if (flag.progressiveWin)
      {
        sendInfoProgBonus (EVT_54_PROGRESSIVE_WIN);
        flag.progressiveWin = 0;
      }
      else if (flag.legacyBonusWin)
      {
        if (sendInfoProgBonus (EVT_7C_LEGACY_BONUS_PAYED))
          flag.legacyBonusWin = 0;
      }
      nextPoll = POLL_SINC1;
      // Reinicia el temporizador, a fin de espaciar el próximo paso de la secuencia de polling
      break;

    case POLL_LP:
      // Hace un LP
      polling ( NEXT_CICLIC_POLL);

      // La secuencia de polling continúa de manera diferenciada si está o no
      // habilitado el progresivo. Si está habilitado siempre intercala un LP1C después de
      // cada LP, a fin de hacer una detección rápida de los cambios en el coin in.
      if ((addr_serv_prog.addr & 0x0000ffff) == (MyIP () & 0x0000ffff)
          && (((addr_serv_prog.addr & 0x00ff0000) >> 16) == 10))
      {
        nextPoll = POLL_SINC_LP1C_PROG;
      }
      else
      {
        nextPoll = POLL_SINC1;
      }
      break;

    case POLL_SINC_LP1C_PROG:
      // Envia un 0x80 para mantener sincronizada a la VGM
      UART0_tx_80();
      nextPoll = POLL_LP1C_PROG;
      break;

    case POLL_LP1C_PROG:
      // Consulta coin in, coin out, jackpot, games won y games played,
      // para reportar cualquier cambio de coin in al servidor de progresivos

      polling (NPOLL_COIN_IN);

      if (games_played != bcd2long (longPollBuffer.lp1c, 8))
      {
        flag.sendCoinIn = 1;
        // Guarda el valor de coin in, para poder ver luego si cambió
        games_played = bcd2long (longPollBuffer.lp1c, 8);

        // Envía los contadores al progresivo
        void sendMeters();
      }
      nextPoll = POLL_SINC1;
      break;

    case POLL_LP1C:
      /* Consulta coin in, coin out, jackpot, drop, total hand paid cancelled credits
         antes de hacer un cashin */
      polling (NPOLL_COIN_IN);

      nextPoll = POLL_SINC3;
      break;

    case POLL_SINC3:
      // Envia un 0x80 para mantener sincronizada a la VGM
      UART0_tx_80();

      // La próxima vez efectúa la operación de cashless requerida
      nextPoll = POLL_CASH;
      break;

    case POLL_CASH:
      nextPoll = poll_cash();
      break;

    case POLL_HANDPAY_52:
      // Verifica si el pago manual fue reseteado manualmente
      // y falta enviar el mensaje al servidor

      reset_handpay ( EVT_52_HANDPAY_RESET);

      if ( flag.cashoutButtonPressed )
      {
        /* El pago manual se inició por haber presionado el botón de COBRAR -->
         --> entonces falta realizar la descarga por retiro de tarjeta y
         no corresponde poner el cartel de "MAQUINA LIBRE / INGRESE TARJETA */
        flag.faltaDescargandoJ = 1;
      }

      if (getCardStatus ())
      {
        if (flag.cashoutButtonPressed)
        {
          // Pago manual requerido por el jugador mediante el botón de pago
          LCD_print1(_RETIRE_TARJETA);
        }
        else
        {
          // Jackpot --> permite seguir jugando
          LCD_print (_MAQUINA_JUGANDO, _BUENA_SUERTE);
        }
      }
      else if (!flag.faltaDescargandoJ)
      {
        LCD_print_maquina_libre();
      }

      flag.handpayRequest = 0;
      flag.cashoutButtonPressed = 0;

      nextPoll = POLL_SINC1;
      break;

    case POLL_TICKET:
      if (ticket_flag.do_ticket_out || ticket_flag.do_tito_handpay_3e)
      {
        if (!waitingAnyServerReply())
          ticket_out ();
      }
      else if (ticket_flag.do_set_validation_id)
      {
        if (tiempo.anio)
          set_validation_id ();
      }
      nextPoll = POLL_SINC1;
      break;

    case POLL_SINC4:
      // Envia un 0x80 para mantener sincronizada a la VGM
      UART0_tx_80();

      // La próxima vez hace una consulta de créditos
      nextPoll = POLL_LP1A;
      break;

    case POLL_LP1A:
      // Después de un cashin, cashout o handpay
      // consulta los créditos actuales para actualizar currentCredits
      polling (NPOLL_CURRENT_CREDITS);

      if (waitingAnyServerReply())
      {
        // Como puede haber más o menos demora para recibir la respuesta
        // del servidor con los créditos a cargar, no reinicia el ciclo de polling
        // hasta que el cashin o cashout no se haya completado
        nextPoll = POLL_SINC4;
      }
      else
      {
        nextPoll = POLL_SINC1;
      }
      break;
    }
  }
}

/**
 * @brief Inicializa la interacción mediante protocolo SAS con la VGM
 * @param None
 * @return None
 */
static void initSAS ()
{
  int i;
  uint8_t evt=0;

  LCD_print(_INICIANDO_SAS, "");

  UART0_tx_80();  // Envía algo para generar actividad en Tx de la UART

  // Bucle para verificar que la máquina funciona y hay comunicación con ella
  while (1)
  {
    DoNetworkStuff();

    Hello_process();        /* Envío periódico del Hello y procesamiento de las respuestas */


    // Espera recibir 5 bytes con la dirección de la VGM, para asegurarse de que esté
    // activa. Como algunas máquinas no envían la dirección con el bit de wakeup seteado,
    // se esperan recibir 5 bytes consecutivos con el mismo valor, para distinguir de
    // cualquier otro mensaje.

    LCD_print (_INICIANDO_SAS, "(1)");

    do
    {
      for (i = 0; i < 5;)
      {
        DoNetworkStuff();

        Hello_process();        /* Envío periódico del Hello y procesamiento de las respuestas */

        if (UART0_rx())
        {
          if (vgmAddr == rx_buffer[0])
          {
            ++i;
          }
          else
          {
            vgmAddr = rx_buffer[0];
            i = 0;
          }
        }
      }
      // Verifica que la dirección de la VGM esté entre 1 y 127 (este control se agregó
      // porque se detectaron máquinas con dirección cero, que no permiten un
      // funcionamiento correcto de los LP y GP).
      if (vgmAddr == 0 || vgmAddr > 127)
      {
        LCD_print ("DIRECCION VGM", "INVALIDA");
      }
    } while (vgmAddr == 0 || vgmAddr > 127);

    // Para sincronizar con la VGM, hace un par de LP
    LCD_print (_INICIANDO_SAS, "(2)");

    UART0_tx_80();
    delay(pollingPeriod);   // Delay de 200 ms (o 40 ms) entre mensajes de SAS

    // Envía un LP 02 (enable game) porque lo necesitan algunas máquinas
    tx_buffer[0] = vgmAddr;
    tx_buffer[1] = 0x02;     // Enable game
    *((uint16_t*) &tx_buffer[2]) = CRC16 (tx_buffer, 2, 0);

    UART0_tx (tx_buffer, 4);

    UART0_rx();                   // Espera la respuesta (que puede ser ACK o NAK)

    delay( pollingPeriod );       // Delay de 200 ms (o 40 ms) entre mensajes de SAS

    for(i=0; i<20 && unidad_denominacion==0; ++i)
    {
      // Hace un LP1F para obtener la denominación de la máquina
      polling( NPOLL_DENOMINATION );
      delay( pollingPeriod );
      UART0_tx_80();
      delay( pollingPeriod );
    }

    if( unidad_denominacion==0 && flag.denominacion_default )
    {
      /* La máquina no informó la denominación --> toma por
         defecto un centavo */
      unidad_denominacion = 1;
    }

    getCurrentCredits (&currentCredits);

    // Identifica si la marca de la máquina es EIBE
    delay( pollingPeriod );
    if ( polling( NPOLL_FEATURES ) > 0 )
    {
      if( strncmp("EE", vgmID, 2) == 0 )
      {
        cfg.eibe = 1;
        cfg.cashlessBolaExtra = 1;
      }
    }
    delay( pollingPeriod );

    // Verifica que -durante un segundo- la VGM no transmita nada
    LCD_print (_INICIANDO_SAS, "(3)");

    if (!UART0_rx())
    {
      strcpy((char *)tx_buffer, "VGM: ");
      strcat((char *)tx_buffer, vgmID);
      LCD_print ("SAS OK", (char *)tx_buffer);
      delay( _2seg );  // Delay para mantener el mensaje en el display

      if( unidad_denominacion )
      {
        sprintf((char *)tx_buffer,"%lu", unidad_denominacion);
        LCD_print( "DENOMINACION", (char *)tx_buffer );
        delay( _1seg );
      }
      else
      {
        sprintf((char *)tx_buffer,"INCORRECTA: %lu", unidad_denominacion);
        LCD_print( "DENOMINACION", (char *)tx_buffer );
        delay( _5seg );
        continue;  // Reinicia el ciclo de inicio de SAS
      }

      flag.vgmAlive = 1;  // La máquina responde correctamente

      break;                // OK --> sale del loop
    }
    else
    {
      sprintf((char *)tx_buffer, "BUFLEN: %d", rx_buflen);
      sprintf((char *)tx_buffer+16, "%02X %02X %02X %02X", rx_buffer[0],rx_buffer[1],rx_buffer[2],rx_buffer[3]);
      LCD_print ((char *)tx_buffer, (char *)tx_buffer+16);
    }
  }
  // Espera conexión con el servidor
  LCD_print ("CONECTANDO", "CON EL SERVIDOR");
  do
  {
    DoNetworkStuff();
    Hello_process();
    UART0_tx_80();
    delay (pollingPeriod);
  } while (!flag.fechaHoraValida);

  /* Pide un evento para verificar si está en modo real time event report, y en tal caso deshabilitarlo.

     Antes simula que la máquina está conectada pero con 20 eventos perdidos, para que si la máquina
     no responde la marque como desconectada y si responde no la marque como reconectada ni ponga
     el cartel de máquina libre.
  */
  flag.vgmAlive = 1;
  lostEvents = 20;

  if( getEvento( &evt ) > 1 )
  {
    delay( pollingPeriod );
    UART0_tx_80();
    delay( pollingPeriod );

    // Deshabilita la generación de eventos en tiempo real
    for( i=0; i<3; ++i )
    {
      if( disableRealTimeEventReport() )
        break;
      delay( pollingPeriod );
    }
    UART0_tx_80();
    delay( pollingPeriod );
  }
  if (evt && filtrarEvento (&evt))
  {
    // Resguarda el evento para que no se pierda si hay un reinicio del módulo
    //backupEvt(evt);

    // Eventos que se reportan al servidor
    bufEventos.evento[bufEventos.nEventos++] = evt;
  }
  // Averigua qué protocolo cashless usa la máquina, por si necesita
  // hacer una descarga al inicio, antes de haber hecho una carga.
  if( cfg.eibe )
    cashProtocol = 'A';   // Las EIBE son siempre AFT
  else if( AFT_getTransactionID( 1 ) )
    cashProtocol = 'A';   // Respondió a una interrogación AFT
  else
    cashProtocol = 'e';   // No respondió a una interrogación AFT --> asume que usa EFT
  delay( pollingPeriod );
  UART0_tx_80();
  delay( pollingPeriod );

  LCD_print_maquina_libre();

  // Si la VGM tiene créditos ordena descargarla
  if ( currentCredits )
  {
    cash_flag = DO_CASHOUT;
  }

  // Por las dudas de que la máquina tenga billetero, lo deshabilita
  DisableBillAcceptor();
}

/**
 * @brief Dentro del ciclo de polling, esta es la función encargada de hacer ejecutar
 *        la operación correspondiente de acuerdo al caso: cashin(), cashout() o reset_handpay()
 */
static int poll_cash (void)
{
  if( flag.handpayRequest )
    // En cashless lo resetea en forma remota
    LCD_print ("ACREDITANDO", "PAGO MANUAL");
  else if (flag.cashoutButtonPressed )
  {
    // Descarga por botón cobrar
    LCD_print ("DESCARGANDO", ((getCardStatus())?_RETIRE_TARJETA:""));
    flag.cashoutButtonPressed = 0;
  }else if( cash_flag == DO_CASHOUT ){
    if( montoVendedor )
      LCD_print( _DESCARGANDO_V, "" );
    else {
      LCD_print( _DESCARGANDO_J, "" );
      flag.faltaDescargandoJ = 0;
    }
  }

  if (flag.handpayRequest)
  {
    // Hay un requerimiento de pago manual (evento 51) --> Intenta resetearlo en forma remota
    // y enviar al servidor el monto descargado

    if (reset_handpay ( EVT_51_HANDPAY_PENDING))
    {
      // En el próximo paso va a actualizar currentCredits
      nextPoll = POLL_SINC4;
    }
    else
    {
      // Falló el reset remoto de pago manual --> vuelve a comenzar el ciclo de polling
      nextPoll = POLL_SINC1;
    }
  }
  else if (cash_flag == DO_CASHOUT && !waitingAnyServerReply())
  {
    // Descarga la VGM (ya sea por retiro de tarjeta o por evento 6A (botón de pago en AFT)
    cashout ();
    if (cash_flag)
    {
      // Falló la descarga. Reinicia el ciclo para intentar más tarde
      nextPoll = POLL_SINC1;
    }
    else
    {
      // Descarga exitosa. En el próximo paso va a actualizar currentCredits
      nextPoll = POLL_SINC4;
    }
  }
  else if (cash_flag == DO_CASHIN && !waitingAnyServerReply())
  {
    // Va a cargar la VGM
    cashin ();
    if (montoVendedor)
    {
      // Si está en el medio de una carga de vendedor,
      // para no demorar de más reinicia el ciclo
      nextPoll = POLL_SINC1;
    }
    else
    {
      // En el próximo paso va a actualizar currentCredits
      nextPoll = POLL_SINC4;
    }
  }
  else
  {
    // Reinicia el ciclo (va a hacer un GP)
    nextPoll = POLL_SINC1;
  }

  return nextPoll;
}

/**
 * @brief Le pide un evento a la VGM.
 *        Obs.: como respuesta a un pedido de eventos puede llegar la respuesta a un
 *        LP21 previo (ROM signature), en ese caso envía la respuesta al servidor.
 * @param[out] evt En *evt deposita el código de evento recibido
 * @return 1 si obtuvo un evento, 0 si no, >1 si la VGM está en modo real time event report.
 */

#define tx ((OnDemandLongPollMsgType *)&rsp)

int getEvento (uint8_t *evt)
{
  int i;
  static OnDemandLongPollMsgType rsp;

  // Lazo for para repetir hasta 3 veces el pedido de eventos.
  // Si a la 3ra vez no se obtuvo respuesta, retorna con 0
  for (i = 0; i < 3; ++i)
  {
    UART0_tx_81();   // pide un evento a la VGM

    // Espera la máxima longitud de un mensaje de evento (15), cuando
    // está habilitado el modo real time event report

    switch (UART0_rx())
    {
    case 0:
      // No hubo respuesta
      break;

    case 1:
      // Respondió en forma normal con el código de evento
      *evt = rx_buffer[0];

      if (!flag.vgmAlive)
      {
        // Se recuperó de un error de conexión, entonces
        // bloquea o desbloquea la máquina, según el último estado registrado,
        // y pone un mensaje en el display para avisar.
        if (flag.lockVGM)
          DisableVGM();
        else
          EnableVGM();

        if ((features.cashless & CASHLESS_ENABLED) && getCardStatus ())
        {
          LCD_print1("RETIRE TARJETA");
        }
        else
        {
          LCD_print("MAQUINA LIBRE", "RETIRE TARJETA");;
        }
        if (bufEventos.nEventos < MAX_EVENTOS)
        {
          bufEventos.evento[bufEventos.nEventos++] = EVT_F1_VGM_CONNECTED;
        }
      }
      lostEvents = 0;
      flag.vgmAlive = 1;      // Notifica que la VGM está conectada y responde
      return 1;

    default:
      // respondió con un mensaje de tipo respuesta a long poll,
      // porque está en modo real time event report, o bien es la respuesta a un LP21 (ROM signature)
      if (rx_buflen >= 5 && !CRC16 (rx_buffer, rx_buflen, 0)
          && rx_buffer[0] == vgmAddr)
      {
        if (rx_buffer[1] == 0x21)
        {
          // Respuesta a un LP21 previo. La VGM lo envía como respuesta a un GP, luego de
          // haber calculado el ROM signature
          tx->length = rx_buflen;
          for (i = 0; i < rx_buflen && i < 128; ++i)
            tx->rsp[i] = rx_buffer[i];
          for (i = 0; i < 3; ++i)
            tx->mac[i] = MyMAC[i];
          tx->uid = uid;
          tx->hash = rand ();
          tx->crc = CRC16 ((uint8_t*) tx, sizeof(OnDemandLongPollMsgType) - 2,
                           0);
          cifrar ((uint8_t*) tx, sizeof(OnDemandLongPollMsgType));

          sendUDP ((uint8_t*) tx, sizeof(OnDemandLongPollMsgType), upcb_cli,
                   &addr_serv_online, SERV_PORT_LP21_ROM_SIGN);

          // Simula haber recibido un 0x00 (no hay eventos)
          *evt = 0x00;
          return 1;
        }
        else if (rx_buffer[1] == 0xff)
        {
          // Evento reportado en modo real time
          *evt = rx_buffer[2];
          lostEvents = 0;

          if (!flag.vgmAlive && bufEventos.nEventos < MAX_EVENTOS)
          {
            bufEventos.evento[bufEventos.nEventos++] = EVT_F1_VGM_CONNECTED;
          }
          flag.vgmAlive = 1;  // Notifica que la VGM está conectada y responde
          return rx_buflen;
        }
      }
      break;
    }
    delay (pollingPeriod);
  }

  if (flag.vgmAlive && ++lostEvents > 20)
  {
    flag.vgmAlive = 0;  // Notifica que la VGM no responde o está desconectada

    if (bufEventos.nEventos < MAX_EVENTOS)
    {
      bufEventos.evento[bufEventos.nEventos++] = EVT_F0_VGM_DISCONNECTED;
    }
  }
  return 0;        // No obtuvo respuesta
}

#undef tx

/**
 * @brief Filtrado de eventos: verifica si un evento debe ser informado al servidor o no.
 *        También puede anular algún evento que no es de interés.
 * @param[in/out] evtPtr apunta al evento a verificar, que luego del filtrado puede quedar igual o ser anulado
 * @return 1 si el evento debe ser reportado al servidor, 0 si no.
 */
int filtrarEvento (uint8_t *evtPtr)
{
  switch (*evtPtr)
  {
  case 0:
    // No hubo evento
    break;
  case EVT_1F_WAITING_USER_INPUT:
    /* Como este evento (cuando la VGM lo genera) se envía permanentemente,
     * para no saturar al módulo y al servidor, se lo anula.
     */
    *evtPtr = 0;
    /* Los siguientes eventos no se reportan al servidor pero no se anulan, porque
     * son procesados por el módulo.
     */
    //  case EVT_6A_AFT_REQ_4_HOST_CASHOUT:
    //  case EVT_6F_GAME_LOCKED:
    //  case AFT_69_EVENT_TRANSFER_COMPLETE:
    //  case EVT_8C_GAME_SELECTED:
    //  case EVT_81_HOPPER_LEVEL_LOW:
  case EVT_7E_GAME_STARTED:
  case EVT_7F_GAME_ENDED:
    break;
  default:
    // Los restantes eventos se deben reportan al servidor
    return 1;
  }
  return 0;
}

/**
 * @brief Borra el buffer donde almacena los eventos e incrementa el número de secuencia
 * @param None
 * @return None
 */
static void clearEventBuffer (void)
{
  int i;
  bufEventos.msgID[0] = 'E';
  bufEventos.msgID[1] = 'V';
  bufEventos.nSec = ++nSecEventMsg;
  bufEventos.nEventos = 0;
  for (i = 0; i < MAX_EVENTOS; ++i)
    bufEventos.evento[i] = 0;
}

/**
 * @brief Realiza un pedido y procesamiento de evento.
 */
static int general_poll (void)
{
  uint8_t evt = 0;  // Por defecto no hay eventos

  // Por defecto, el próximo paso es un sincronismo previo a un LP
  nextPoll = POLL_SINC2;

  if (bufEventos.nEventos < MAX_EVENTOS)
  {
    // Pide un evento a la máquina
    if (evt || getEvento (&evt))
    {
      if (filtrarEvento (&evt))
      {
        // Resguarda el evento para que no se pierda si
        // hay un reinicio del módulo
        //backupEvt(evt);

        // Eventos que se reportan al servidor
        bufEventos.evento[bufEventos.nEventos++] = evt;
      }
      // Averigua si es algún evento vinculado a un pago manual,
      // que en algunos casos se usa para alterar algunas condiciones.
      // También registra si se inició o finalizó una jugada, porque este dato
      // se utiliza en las máquinas que permiten la recarga de créditos para
      // una bola extra (EIBE y otras)
      switch (evt)
      {
      case EVT_7E_GAME_STARTED:
        // Si se presionó el botón de pago (evt 66) y no hay crédito suficiente
        // la VGM no envía el evt 51 y el módulo queda bloqueado hasta que se
        // agote el crédito. Si luego del evt 66 se inicia una jugada, resetea
        // la condición de espera del evt 51.
        flag.cashoutButtonPressed = 0;
        // Como inicialmente el mensaje de máquina en juego también indica el saldo,
        // luego de comenzar a jugar lo borra
        if ( getCardStatus() )
        {
          /* Para cambiar el mensaje en el display verifica que esté puesta una tarjeta, porque
           en las ruletas electrochance genera el evento 7E cada vez que inicia una partida,
           aunque el puesto no tenga crédito ni esté apostando. */
          LCD_print (_MAQUINA_JUGANDO, _BUENA_SUERTE);
        }
        // Registra que está en el medio de una jugada
        flag.VGMjugando = 1;
        break;
      case EVT_7F_GAME_ENDED:
        // En el caso de las máquinas de bingo, si finaliza una jugada estando en medio
        // de una partida y no hay una tarjeta presente en el lector, entonces
        // fuerza una descarga porque asume que el cliente se retiró en medio de la partida
        if ( flag.VGMjugando && (features.cashless & CASHLESS_ENABLED_BOLA_EXTRA) && flag.cardStatus && !getCardStatus () )
        {
          cash_flag = DO_CASHOUT;
          flag.cashoutButtonPressed = 1;
          flag.cardStatus = 0;
        }
        // Registra que finalizó una jugada
        flag.VGMjugando = 0;
        break;
      case EVT_66_CASHOUT_BUTTON_PRESSED:
        // Fue presionado el boton de pago.
        // Obs.: como hay máquinas que generan este evento aunque no tengan crédito,
        // se consulta también por currentCredits.
        //flag.cashoutButtonPressed = ((currentCredits)?1:0);
        break;
      case EVT_6A_AFT_REQ_4_HOST_CASHOUT:
        // Fue presionado el boton de pago, y como consecuencia genera este evento,
        // que indica que la máquina está a la espera de que la descarguen.
        cash_flag = DO_CASHOUT;
        flag.cashoutButtonPressed = 1;

        // Registra que finalizó una jugada. Esto tiene impacto en las máquinas de bingo,
        // porque mientras está en la condición de bola extra puede darse el caso de que
        // venza el timeout o bien el usuario cancele la jugada, y entonces llegue el evento
        // 6A y luego el 7F.
        flag.VGMjugando = 0;
        break;
      case EVT_6B_AFT_REQ_4_HOST_CASHOUT_WIN:
        /* La máquina generó este evento, que indica que la máquina
         está a la espera de que le descarguen un premio. */
        cash_flag = DO_CASHOUT;
        flag.cashoutWin = 1;
        break;
      case EVT_51_HANDPAY_PENDING:
        // En el próximo LP manda a resetear el pago manual (o a obtener el monto
        // del pago manual, en el caso de una máquina con online solamente)
        flag.handpayRequest = 1;
        flag.waitingEvent52 = 1;
        break;
      case EVT_52_HANDPAY_RESET:
        // En el próximo LP va a verificar si el pago manual fue reseteado
        //  manualmente y falta enviar el mensaje al servidor
        nextPoll = POLL_HANDPAY_52;
        break;
      case EVT_54_PROGRESSIVE_WIN:
        // Se pagó un premio progresivo y se lo debe informar
        flag.progressiveWin = 1;
        break;
      case EVT_7C_LEGACY_BONUS_PAYED:
        // Se pagó un premio legacy bonus y se lo debe informar
        flag.legacyBonusWin = 1;
        break;
      case EVT_3E_HANDPAY_HAS_BEEN_VALIDATED:
        // La VGM efectuó un pago manual sin emisión de ticket. Obtiene los datos
        // de la misma manera que cuando se emitió un ticket.
        // Por ahora (12/3/2014) no se lo informa al servidor.
        flag.handpayRequest = 0;
        ticket_flag.do_tito_handpay_3e = 1;
        break;
      case EVT_3D_CASHOUT_TICKET_PRINTED:
        // No verifica que esté TITO habilitado, porque en caso de que la VGM esté
        // configurada para pagar con tickets, debe hacerse el LP correspondiente
        // para que la VGM borre el ticket del buffer (Obs.: las Bally se bloquean
        // si se llene este buffer).
        flag.handpayRequest = 0;
        ticket_flag.do_ticket_out = 1;
        break;
      case EVT_3F_VALIDATION_ID_NOT_CONFIGURED:
        /* Como hay máquinas configuradas dual, para TITO y cashless, siempre se
         responde a este evento, enviando la validación requerida */
        ticket_flag.do_set_validation_id = 1;
        break;
      case EVT_71_CHANGE_LAMP_ON:
        getCurrentCredits(&currentCredits);
        if( currentCredits )
        {
          mensaje_al_bar(evt);
          flag.llamado_al_bar = 1;
        }
        break;
      case EVT_72_CHANGE_LAMP_OFF:
        if( flag.llamado_al_bar )
        {
          mensaje_al_bar(evt);
          flag.llamado_al_bar = 0;
        }
        break;
      }

      // Algunos eventos (abrir y cerrar puerta)
      // implican poner un mensaje en el display
      switch (evt)
      {
      case EVT_11_MAIN_DOOR_OPEN:
        flag.doorOpen = 1;
        LCD_print (_NO_FUNCIONA, _PUERTA_ABIERTA);
        break;
      case EVT_12_MAIN_DOOR_CLOSED:
        flag.doorOpen = 0;
        if (flag.cardStatus || currentCredits)
          LCD_print ( _MAQUINA_JUGANDO, _BUENA_SUERTE );
        else
          LCD_print_maquina_libre ();
        break;
      }
    }
    else
    {
      // No obtuvo respuesta al pedido de eventos (repetido 3 veces)
      // --> manda a reiniciar el ciclo con un 0x80
      nextPoll = POLL_SINC1;
    }
  }

  // Si obtuvo un evento vuelve a repetir el pedido, excepto que se trate de
  // algunos eventos que requieren de atención inmediata
  switch (evt)
  {
  case EVT_51_HANDPAY_PENDING:
  case EVT_52_HANDPAY_RESET:
  case EVT_54_PROGRESSIVE_WIN:
  case EVT_7C_LEGACY_BONUS_PAYED:
  case EVT_6A_AFT_REQ_4_HOST_CASHOUT:
  case EVT_6B_AFT_REQ_4_HOST_CASHOUT_WIN:
  case EVT_3D_CASHOUT_TICKET_PRINTED:
  case EVT_3E_HANDPAY_HAS_BEEN_VALIDATED:
  case EVT_3F_VALIDATION_ID_NOT_CONFIGURED:
    // continúa el ciclo de polling por donde corresponda a cada evento
    break;
  default:
    // Si la VGM no tiene más eventos o bien se llenó el buffer, envia los que tenga acumulados
    if (!waitingAnyServerReply() && ((!evt && bufEventos.nEventos) || bufEventos.nEventos >= MAX_EVENTOS))
    {
      // Envía los eventos al servidor y se bloquea hasta recibir el ACK
      sendEvents();
    }
    else if (evt && bufEventos.nEventos < MAX_EVENTOS)
      nextPoll = POLL_SINC1;
  }

  if (!flag.vgmAlive)
  {
    // No hay conexión con la VGM, entonces vuelve a iniciar el ciclo
    nextPoll = POLL_SINC1;
  }

  return nextPoll;
}

/**
 * @brief Envía el mensaje de eventos y espera la respuesta
 */
void sendEvents()
{
  int j;
  static EventMsgType tmpBufEventos;
  uint32_t t0;

  flag.waitingEventAck = 1;  // Setea el flag de espera del ACK

  // Primero estampa fecha y hora
  sprintf (bufEventos.fechaHora, "%04u%02d%02d%02d%02d%02d", tiempo.anio,
           tiempo.mes, tiempo.dia, tiempo.hora, tiempo.min, tiempo.seg);

  // Agrega la MAC del módulo y el UID de la VGM
  for (j = 0; j < 3; ++j)
    bufEventos.mac[j] = MyMAC[j];

  bufEventos.uid = uid;

  // Agrega código de hash
  bufEventos.hash = rand ();

  do
  {
    // Calcula el CRC
    bufEventos.crc = CRC16 ((unsigned char*) &bufEventos, sizeof(EventMsgType) - 2, 0);
    // Hace una copia del buffer de eventos para transmitirlo cifrado
    tmpBufEventos = bufEventos;
    // cifra el mensaje sobre el buffer udp
    cifrar ((uint8_t*) &tmpBufEventos, sizeof(EventMsgType));
    // Envía el mensaje al servidor
    sendUDP ((uint8_t*) &tmpBufEventos, sizeof(EventMsgType), upcb_cli, &addr_serv_online, SERV_PORT_EVENTS);

    // Espera hasta 5 s por la recepción del ACK. Si no, retransmite.
    t0 = HAL_GetTick();
    while( flag.waitingEventAck && (HAL_GetTick()-t0)<_3seg )
    {
      MX_LWIP_Process();
      delay(pollingPeriod);
      UART0_tx_80();
    }
  }while( flag.waitingEventAck );

  // Si corresponde, desbloquea la VGM
  if ( !flag.lockVGM )
    EnableVGM();
}

/**
 * @brief Inicializa variables globales
 */
static void initVars()
{
  flag.setVGMDateTime = 1;
  flag.fechaHoraValida = 0; // Inicialmente fuerza a esperar los parámetros que vienen
  // en el Hello, antes de empezar a procesar SAS

  // Setea los flags indicadores de espera de alguna respuesta del servidor
  flagsWaitingServerReply.waitingEventAck = 1;
  flagsWaitingServerReply.waitingCashinReply = 1;
  flagsWaitingServerReply.waitingCashoutReply = 1;
  flagsWaitingServerReply.waitingCashinNakEnd = 1;
  flagsWaitingServerReply.waitingHandpayReply = 1;
  flagsWaitingServerReply.waitingBonusCashinReply = 1;
  flagsWaitingServerReply.waitingBonusCashoutReply = 1;
  flagsWaitingServerReply.waitingBonusNakEnd = 1;
  flagsWaitingServerReply.waitingHelloReply = 1;

  /* Estas variables pueden cambiar dependiendo del tipo de máquina a la que
   * se vaya a conectar el módulo.
   */
  cfg.eibe = 0;
  cfg.cashlessBolaExtra = 0;

  uid = 345;
}


#if 0
void myMain(void)
{
  //int nextPoll = POLL_SINC;
  int i, j=0;
  //unsigned char tx_msg[] = {1,2,3,4,5,6,7,8,9,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
  char tx_msg[]  = {"HOLA MUNDO 1234\0HELLO WORLD 567"};
  char tx_msg2[] = {"1234567899ABCDE\0QWERTYUIOPASDFG"};

  /* STMPS2141STR: EN activo en bajo — forzar PC0 a 0 para activar VBUS */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_Delay(1000);

  stimer_start(&timer_tx, 1000);
  stimer_start(&timer_sas_polling, 200);

  /* Infinite loop */
  while (1)
  {
    MX_LWIP_Process();      /* procesar stack LWIP — incluye eth rx/tx */
    UDP_Echo_Process();     /* reservado para lógica futura */

    //MX_USB_HOST_Process();  /* ── Procesar USB Host ── */

    checkCardReader();
    // SOLO PARA PRUEBA
    if( cash_flag == DO_CASHIN )
      LCD_print((char *)cardID, (char *)cardID+16);
    else if( cash_flag == DO_CASHOUT )
      LCD_print("","");
    cash_flag = 0;
    // FIN PRUEBA

/*
    if( stimer_expired(&timer_sas_polling) )
    {
      switch( nextPoll )
      {
      case POLL_SINC1:
        nextPoll = POLL_GP;
        break;
      }
      //HAL_UART_Transmit(&huart5, msg, sizeof(msg), HAL_MAX_DELAY);
    }
*/

    if( stimer_expired(&timer_tx) )
    {
      switch( ++j%4 )
      {
      case 0:
        UART0_tx( (uint8_t *)tx_msg, sizeof(tx_msg) );
        break;
      case 1:
        UART0_tx( (uint8_t *)tx_msg2, sizeof(tx_msg2) );
        break;
      case 2:
        UART0_tx_81(1);
        break;
      default:
        UART0_tx_80();
        break;
      }

      if( UART0_rx() > 0 )
      {
        if( rx_buflen == 1 )
        {
          char txt[4];
          sprintf(txt, "%02X", rx_buffer[0]);
          LCD_print(txt,"");
        }
        else if( rx_buflen > 1 )
          LCD_print((char *)rx_buffer, (char *)rx_buffer+16);
        else
          LCD_print("","");
      }
      rx_buflen = 0;

      for(i=0;i<sizeof(rx_buffer);++i)
        rx_buffer[i] = 0;
    }
  }
}
#endif

/**
  * @brief  TIM6: gap de 40ms detectado
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        //flag_gap_detected = 1;
        HAL_TIM_Base_Stop_IT(&htim6);  /* detener hasta el próximo byte */
    }
}

/**
  * @brief  EXTI0: pulsador de usuario presionado
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)   /* BTN_USER = PA0 */
    {
        /* Debounce simple por software */
        //HAL_Delay(50);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
        {
          // Hacer algo cuando se presionó el botón de usuario
        }
    }
}

/**
 * @brief  Procesa un mensaje recibido de ACK de eventos
 * @param
 * @return None
 */
void processEventAck (struct pbuf *udp_rx_buf)
{
  EventAckType *p = udp_rx_buf->payload;

  if (udp_rx_buf->len == sizeof(EventAckType))
  {
    decifrar((uint8_t* )p, sizeof(EventAckType));

    if (CRC16 ((uint8_t*) p, sizeof(EventAckType), 0) == 0 && p->msgID[0] == 'E'
        && p->msgID[1] == 'V')
    {
      /* Mensaje recibido sin errores --> lo procesa */
      if (p->nSec == nSecEventMsg)
      {
        if (p->codError == 0 || (p->codError == 1 && eventMsgRetry))
        {
          /* Recibió 0x00 (OK), o bien 0x01 (Nro de Sec repetido) luego de una
           * retransmisión. En este último caso se asume que el mensaje fue recibido
           * pero se perdió el ACK, por lo que al retransmitir el servidor lo ve
           * como Nro de secuencia repetido */

          clearEventBuffer (); // borra el buffer de eventos e incrementa el nro de secuencia
          eventMsgRetry = 0;   // resetea el flag de retransmisión de eventos

          // Resetea el backup de eventos en EEPROM
          //        eraseEvt();

          /* Desbloquea a la tarea de envío del mensaje de eventos, para que finalice
           el ciclo actual de envío y espera del ACK */
          flag.waitingEventAck = 0;
        }
        else if (p->codError == 1)
        {
          // Nro de Secuencia repetido (y no es una retransmisión)

          // Incrementa el N° de secuencia en el buffer de transmisión
          // y resetea el flag de espera de ACK, para que prepare otra transmisión.
          bufEventos.nSec = ++nSecEventMsg;

          eventMsgRetry = 0;      // resetea el flag de retransmisión de eventos

          /* No levanta el semáforo que tiene bloqueada a la tarea de envío de
           * mensajes de eventos. Al vencimiento del timeout enviará un nuevo mensaje,
           * esta vez con los parámetros adecuados
           */
        }
      }
    }
  }
}

/*
 * Envía un comando de enable o disable de alguna función de la VGM.
 * Obs.: esta función debe ser llamada desde dentro del bucle de la tarea SAS,
 * porque sigue los pasos de la secuencia de polling SAS.
 */
int enableDisableLP (uint8_t cmd)
{
  int i = 1;
  uint8_t buf[4];
  uint16_t crc;

  if (!cfg.eibe)
  {
    delay (pollingPeriod);

    buf[0] = vgmAddr;
    buf[1] = cmd;
    crc = CRC16 (buf, 2, 0);
    buf[2] = crc & 0x00ff;
    buf[3] = (crc >> 8) & 0x00ff;

    // transmite
    UART0_tx(buf,4);

    // espera respuesta
    if (!UART0_rx () || (rx_buffer[0] & 0x7f) != vgmAddr)
      i = 0;         // No tuvo respuesta o resultado incorrecto

    if (cmd == CMD_LOCK_VGM)
    {
      LCD_print ("MAQUINA", "BLOQUEADA");
    }
  }
  delay (pollingPeriod);
  UART0_tx_80();
  delay (pollingPeriod);

  return i;
}

//------------------------------------------------------
// Deshabilita la generación de eventos en tiempo real
// Retorna 1 si tuvo éxito, 0 si no
//------------------------------------------------------
static uint8_t disableRealTimeEventReport( void )
{
  uint8_t buf[5];
  uint16_t crc;

  buf[0]=vgmAddr;
  buf[1]=0x0E;
  buf[2]=0x00;
  crc=CRC16( buf,3,0 );
  buf[3]=crc & 0x00ff;
  buf[4]=( crc >> 8 ) & 0x00ff;

  // transmite
  UART0_tx( buf, 5 );

  // espera respuesta
  if( !UART0_rx() || rx_buffer[0] != vgmAddr )
    return 0;          // No tuvo respuesta o CRC incorrecto

  return 1;
}

/*
 * Realiza un bucle de espera por un cierto tiempo, enviando 0x80 a la VGM
 * para mantenerla sincronizada.
 */
void delay80 (uint32_t delayTime)
{
  uint32_t t = pollingPeriod;

  delay (pollingPeriod);

  while (t < delayTime)
  {
    UART0_tx_80();
    delay (pollingPeriod);
    t += pollingPeriod;
  }
}

//--------------------------------------------------------------------
// Informa el pago de un premio por progresivo o legacy bonus
// al servidor de online
// Parámetro: <evt> indica que evento originó el llamado a la función
//            evt == 0x54 (EVT_54_PROGRESSIVE_WIN)
//            evt == 0x7C (EVT_7C_LEGACY_BONUS_PAYED)
// Retorna: 1 OK
//          0 Error
//--------------------------------------------------------------------

#define pp ((LastProgressiveWinType *)rx_buffer)
#define pb ((LegacyBonusWinInfoType *)rx_buffer)

static unsigned char sendInfoProgBonus (unsigned char evt)
{
  uint32_t j, monto, t0;

  // Carga los campos del mensaje UDP que son comunes a ambos tipos de consultas
  prepareHandpayMsgBuffer ();

  // Obtiene datos del tipo de premio solicitado

  if (evt == EVT_54_PROGRESSIVE_WIN)
  {
    // Consulta por el último premio por progresivo pagado
    for (j = 0; j < 3; ++j)
    {
      tx_buffer[0] = vgmAddr;
      tx_buffer[1] = 0x84;
      UART0_tx (tx_buffer, 2);

      if (UART0_rx()
          && rx_buflen == sizeof(LastProgressiveWinType)
          && !CRC16 (rx_buffer, rx_buflen, 0) && pp->vgmAddr == vgmAddr
          && pp->command == 0x84)
        break;
    }

    if (j > 2)
      return 0;     // No hubo respuesta al LP 84 --> aborta

    // Obtiene el monto y otros parámetros del premio pagado y
    // prepara el mensaje que enviará al servidor

    cashMsgBuffer.evt = evt;
    cashMsgBuffer.progGroup = pp->progGroup;
    cashMsgBuffer.progLevel = pp->progLevel;

    monto = bcd2long ( pp->amount, 10);
  }
  else if (evt == EVT_7C_LEGACY_BONUS_PAYED)
  {
    // Consulta por el último legacy bonus pagado
    for (j = 0; j < 3; ++j)
    {
      tx_buffer[0] = vgmAddr;
      tx_buffer[1] = 0x90;
      UART0_tx (tx_buffer, 2);

      if (UART0_rx()
          && rx_buflen == sizeof(LegacyBonusWinInfoType)
          && !CRC16 (rx_buffer, rx_buflen, 0) && pb->vgmAddr == vgmAddr
          && pb->command == 0x90)
        break;
    }

    if (j > 2)
      return 0;     // No hubo respuesta al LP 90 --> aborta

    // Obtiene el monto y otros parámetros del premio pagado y
    // prepara el mensaje que enviará al servidor

    cashMsgBuffer.evt = evt;
    cashMsgBuffer.multiplier = pb->multiplier;
    cashMsgBuffer.tax_status = pb->tax_status;

    monto = bcd2long ( pb->bonus_amount, 8);
  }
  else
  {
    return 0;   // Pedido erróneo
  }
  // Termina de preparar el mensaje y lo envía
  sprintf ((char*) cashMsgBuffer.monto, "%010lu", monto);

  prepareCashMsgBuffer (); // Carga mac, uid, hash y crc. Luego cifra el mensaje

  do
  {
    // Envía mensaje al servidor de online, para que registre el monto del premio pagado
    sendUDP ((uint8_t*) &cashMsgBuffer, sizeof(cashMsgBuffer), upcb_cli,
             &addr_serv_online, SERV_PORT_PAGO_MANUAL);

    flag.waitingHandpayReply = 1;

    t0 = HAL_GetTick();
    while( flag.waitingHandpayReply && (HAL_GetTick() - t0) < _3seg )
    {
      MX_LWIP_Process();
      delay(pollingPeriod);
      UART0_tx_80();
    }
  } while (flag.waitingHandpayReply);

  return 1;
}
#if 0
#undef pp
#undef pb

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
#endif

//----------------------------------
// Envía mensaje a la barra
//----------------------------------
static void mensaje_al_bar( uint8_t evt )
{
  if( features.IP_server_extra )
  {
    sprintf( (char *)tx_buffer, "%u/%02x", features.uid, evt );
    sendUDP( tx_buffer, strlen((char *)tx_buffer)+1, upcb_cli, &addr_serv_extra, SERV_PORT_BUFFET);
    LCD_print( "LLAMADO AL BAR", evt==EVT_71_CHANGE_LAMP_ON?_ESPERE:"CANCELADO" );
  }
}


