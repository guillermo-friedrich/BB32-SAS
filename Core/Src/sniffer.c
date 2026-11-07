/*
 * sniffer.c
 *
 *  Created on: Sep 24, 2026
 *      Author: guillermo
 */
#if 0
#include "main.h"
#include "fatfs.h"
#include "usb_host.h"
#include "lwip.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "ds2401.h"
#include "udp_echo.h"
#include <stdio.h>
#include <string.h>
#include "sniffer.h"
#include "lcd.h"
#include "card.h"
#include "cashless.h"

volatile union FLAGS flag, flagsWaitingServerReply;

/* ── FatFS ── */
FATFS    USBFatFS;
FIL      MyFile;
FRESULT  fres;
extern char USBHPath[4];
extern ApplicationTypeDef Appli_state;
extern USBH_HandleTypeDef hUsbHostFS;

/* ── UART / DMA ── */
#define UART_DMA_BUF_SIZE   256
uint8_t  uart_dma_buf[UART_DMA_BUF_SIZE];   /* buffer circular DMA */
uint16_t uart_dma_head = 0;                  /* última posición procesada */

/* ── Buffer de línea para FatFS ── */
#define LINE_BUF_SIZE  512
char     line_buf[LINE_BUF_SIZE];            /* línea en construcción "HH HH HH ..." */
uint16_t line_pos  = 0;                      /* posición dentro de line_buf */
uint8_t  line_has_data = 0;                  /* hay bytes pendientes de volcar */

#define MS_X_DIA 86400000  // milisegundos por día
#define MS_X_HORA 3600000  // milisegundos por hora
#define MS_X_MINUTO 60000  // milisegundos por minuto

/* ── Flags de estado ── */
volatile uint8_t  flag_gap_detected  = 0;   /* TIM6 disparó: gap > 40ms */
volatile uint8_t  flag_btn_unmount   = 0;   /* usuario pide desmontar */
volatile uint8_t  flag_usb_ready     = 0;   /* pendrive montado y listo */
volatile uint8_t  flag_usb_mounted   = 0;   /* f_mount exitoso activo */

/* ── Máquina de estados ── */
typedef enum {
    STATE_WAIT_USB,          /* esperando pendrive */
    STATE_MOUNT,             /* montar FS */
    STATE_OPEN_FILE,         /* abrir archivo de log */
    STATE_LOGGING,           /* recibiendo y logueando */
    STATE_FLUSH_AND_CLOSE,   /* volcar último renglón y cerrar */
    STATE_UNMOUNTED,         /* desmontado, LED azul encendido */
    STATE_ERROR              /* error, LED rojo encendido */
} AppState_t;

AppState_t app_state = STATE_WAIT_USB;

extern HCD_HandleTypeDef hhcd_USB_OTG_FS;  /* handle del controlador USB */

typedef struct {
    uint32_t start;      /* valor de HAL_GetTick() al arrancar */
    uint32_t interval;   /* intervalo en ms */
    uint8_t  running;    /* 1 = activo */
} SoftTimer_t;

SoftTimer_t timer_led_blink;   /* ejemplo: parpadeo LED en error */
SoftTimer_t timer_sync;        /* sync periódico a pendrive */
SoftTimer_t timer_tx;          /* Inicio de transmisión */

uint32_t usb_connect_timeout = 0;

/* ─────────────────────────────────────────────
 * LED helpers
 * ───────────────────────────────────────────── */
static void leds_all_off(void)
{
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15,
                      GPIO_PIN_RESET);
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
 * Agregar byte al buffer de línea como "HH "
 * ───────────────────────────────────────────── */
static void line_add_byte(uint8_t byte)
{
  uint32_t d, h, m, s, ticks;

  if (line_pos + 4 >= LINE_BUF_SIZE)
  {
    /* Buffer lleno — forzar flush aunque no haya gap */
    flag_gap_detected = 1;
    return;
  }
  if( !line_has_data )
  {
    // Si la línea está vacía pone una marca de tiempo en primer lugar
    ticks = HAL_GetTick();
    d = ticks / MS_X_DIA;
    ticks = ticks % MS_X_DIA;
    h = ticks / MS_X_HORA;
    ticks = ticks % MS_X_HORA;
    m = ticks / MS_X_MINUTO;
    ticks = ticks % MS_X_MINUTO;
    s = ticks / 1000;
    ticks = ticks % 1000;

    line_pos = snprintf(&line_buf[0], LINE_BUF_SIZE, "%2lu:%02lu:%02lu:%02lu.%03lu: ", d, h, m, s, ticks);
  }

  line_pos += snprintf(&line_buf[line_pos],
                       LINE_BUF_SIZE - line_pos,
                       "%02X ", byte);
  line_has_data = 1;

  /* Toggle LED verde por cada byte */
  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
}

/* ─────────────────────────────────────────────
 * Volcar línea actual al archivo y empezar nueva
 * ───────────────────────────────────────────── */
static void flush_line(void)
{
    if (!line_has_data) return;

    /* Reemplazar último espacio por \r\n */
    if (line_pos >= 1 && line_buf[line_pos - 1] == ' ')
        line_pos--;                /* quitar espacio trailing */

    line_buf[line_pos++] = '\r';
    line_buf[line_pos++] = '\n';

    uint32_t bw;
    fres = f_write(&MyFile, line_buf, line_pos, (void*)&bw);

    if (fres != FR_OK || bw != line_pos)
    {
        /* Error de escritura — cambiar estado */
        app_state = STATE_ERROR;
    }
    else
    {
        /* f_sync para asegurar que los datos lleguen al pendrive */
        f_sync(&MyFile);
    }

    /* Resetear buffer de línea */
    line_pos     = 0;
    line_has_data = 0;
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

/**
  * @brief  The application entry point.
  * @retval void
  */
void myMain(void)
{
  static unsigned char msg[] = {0x0A,0x00,0x00,0x29,0x87,0x42,0x50,0x76,0x92,0x64,0x83,0x34,0x59,0x32,0x20,0x00,0x20,0x00,0x00,0x01,0x54,0x93,0x92,0x00,0x54,0x81,0x88,0x00,0x00,0x00,0x39,0x00,0x00,0x00,0x00,0x32,0x30,0x32,0x35,0x31,0x30,0x30,0x38,0x31,0x32,0x32,0x35,0x32,0x37,0x00,0x34,0x59,0x32,0x20,0x81,0x64,0x54,0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x31,0x36,0x32,0x37,0x33,0x38,0x39,0x39,0x35,0x32,0x00,0x00,0x00,0x00,0x00,0x16,0x00,0xE4,0x03,0x3F,0xD4,0xD3,0x47};

  /* STMPS2141STR: EN activo en bajo — forzar PC0 a 0 para activar VBUS */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_Delay(1000);

  stimer_start(&timer_tx, 1000);

  /* Infinite loop */
  while (1)
  {
    MX_LWIP_Process();      /* procesar stack LWIP — incluye eth rx/tx */
    UDP_Echo_Process();     /* reservado para lógica futura */

    checkCardReader();
    if( cash_flag == DO_CASHIN )
      LCD_print(cardID,cardID+16);
    else if( cash_flag == DO_CASHOUT )
      LCD_print("","");
    cash_flag = 0;

    if( stimer_expired(&timer_tx) )
    {
      HAL_UART_Transmit(&huart5, msg, sizeof(msg), HAL_MAX_DELAY);
    }

    /* ── Procesar USB Host ── */
    MX_USB_HOST_Process();

    /* ── Actualizar flag desde middleware ── */
    if (Appli_state == APPLICATION_READY  && !flag_usb_ready)
      flag_usb_ready = 1;

    if (Appli_state == APPLICATION_DISCONNECT)
    {
      flag_usb_ready   = 0;
      flag_usb_mounted = 0;
      if (app_state != STATE_ERROR)
        app_state = STATE_WAIT_USB;
    }

    /* ══════════════════════════════════════
     * MÁQUINA DE ESTADOS PRINCIPAL
     * ══════════════════════════════════════ */
    switch (app_state)
    {
    /* ── Esperando pendrive ── */
    case STATE_WAIT_USB:
      leds_all_off();
      led_set(GPIO_PIN_14, GPIO_PIN_SET);   /* LED rojo: sin pendrive */

      if (flag_usb_ready)
        app_state = STATE_MOUNT;
      break;

      /* ── Montar sistema de archivos ── */
    case STATE_MOUNT:
      fres = f_mount(&USBFatFS, (TCHAR const*)USBHPath, 1);
      if (fres == FR_OK)
      {
        flag_usb_mounted = 1;
        leds_all_off();
        led_set(GPIO_PIN_13, GPIO_PIN_SET);  /* LED naranja: montado */
        app_state = STATE_OPEN_FILE;
      }
      else
      {
        app_state = STATE_ERROR;
      }
      break;

      /* ── Abrir / crear archivo de log ── */
    case STATE_OPEN_FILE:
    {
      /* Abrir log.txt si existe, crearlo si no existe.
       * FA_OPEN_APPEND posiciona el puntero al final automáticamente. */
      fres = f_open(&MyFile, "log.txt", FA_OPEN_APPEND | FA_WRITE);

      if (fres == FR_OK)
      {
          /* Separador visual entre sesiones */
          const char separator[] = "\r\n=== Nueva sesion ===\r\n\r\ndd:hh:mm:ss.ms\r\n";
          uint32_t bw;
          f_write(&MyFile, separator, strlen(separator), (void*)&bw);
          f_sync(&MyFile);

          HAL_UART_Receive_DMA(&huart5, uart_dma_buf, UART_DMA_BUF_SIZE);
          stimer_start(&timer_sync, 5000);
          app_state = STATE_LOGGING;
      }
      else
      {
          app_state = STATE_ERROR;
      }
      break;
    }
#if 0 // USA NOMBRES CONSECUTIVOS. Lo dejo comentado por si en otro momento resulta útil
      /* Nombre con índice rotativo: log00.txt .. log99.txt */
    {
      char fname[16];
      uint8_t idx = 0;
      do {
        snprintf(fname, sizeof(fname), "log%02u.txt", idx++);
      } while (f_stat(fname, NULL) == FR_OK && idx < 100);

      fres = f_open(&MyFile, fname, FA_CREATE_ALWAYS | FA_WRITE);
      if (fres == FR_OK)
      {
        /* Arrancar recepción DMA circular */
        HAL_UART_Receive_DMA(&huart5,
                             uart_dma_buf,
                             UART_DMA_BUF_SIZE);
        app_state = STATE_LOGGING;
      }
      else
      {
        app_state = STATE_ERROR;
      }
      break;
    }
#endif

    /* ── Recibiendo y logueando ── */
    case STATE_LOGGING:
    {
      /* ── Leer bytes nuevos del buffer DMA circular ── */
      uint16_t dma_tail = UART_DMA_BUF_SIZE
          - __HAL_DMA_GET_COUNTER(huart5.hdmarx);

      while (uart_dma_head != dma_tail)
      {
        uint8_t byte = uart_dma_buf[uart_dma_head];
        uart_dma_head = (uart_dma_head + 1) % UART_DMA_BUF_SIZE;

        line_add_byte(byte);

        /* Reiniciar timer de gap en cada byte recibido */
        gap_timer_restart();
      }

      /* ── Detectar gap > 40ms → nueva línea ── */
      if (flag_gap_detected)
      {
        led_toggle(LED_GREEN_Pin);  /* LED verde parpadeando: RX */

        flag_gap_detected = 0;
        gap_timer_stop();
        flush_line();
      }

      /* ── Pulsador: solicitar desmontaje ── */
      if (flag_btn_unmount)
      //if( HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == 0 )
      {
        flag_btn_unmount = 0;
        app_state = STATE_FLUSH_AND_CLOSE;
      }

      /* ── Desconexión inesperada durante logging ── */
      if (Appli_state == APPLICATION_DISCONNECT)
      {
          /* No podemos hacer f_close() — el pendrive ya no está.
           * Solo limpiar estado interno de FatFS. */
          HAL_UART_DMAStop(&huart5);
          gap_timer_stop();
          stimer_stop(&timer_sync);

          f_mount(NULL, (TCHAR const*)USBHPath, 0);  /* limpiar estado FatFS */
          flag_usb_mounted = 0;

          /* Los datos del último f_sync() están a salvo.
           * Lo que había en RAM desde el último sync se perdió. */
          leds_all_off();
          led_set(GPIO_PIN_14, GPIO_PIN_SET);  /* LED rojo: error/desconexión */
          app_state = STATE_WAIT_USB;
      }
      break;
    }

    /* ── Cerrar archivo y desmontar ── */
    case STATE_FLUSH_AND_CLOSE:
      HAL_UART_DMAStop(&huart5);
      gap_timer_stop();
      stimer_stop(&timer_sync);

      /* 1. Volcar lo que quede en el buffer de línea */
      flush_line();

      /* 2. Cerrar el archivo — hace f_sync implícito + actualiza FAT y directorio */
      fres = f_close(&MyFile);
      if (fres != FR_OK)
      {
          /* f_close falló — intentar sync de emergencia */
          f_sync(&MyFile);
          app_state = STATE_ERROR;
          break;
      }

      /* 3. Desmontar — completa cualquier operación pendiente en la capa MSC */
      f_mount(NULL, (TCHAR const*)USBHPath, 0);
      flag_usb_mounted = 0;

      leds_all_off();
      led_set(GPIO_PIN_15, GPIO_PIN_SET);  /* LED azul: listo para retirar */
      app_state = STATE_UNMOUNTED;
      break;

      /* ── Pendrive desmontado, esperando que lo retiren ── */
    case STATE_UNMOUNTED:
      /* Cuando se desconecte físicamente volvemos a WAIT */
      if (Appli_state == APPLICATION_DISCONNECT)
      {
        leds_all_off();
        app_state = STATE_WAIT_USB;
      }
      break;

      /* ── Error ── */
    case STATE_ERROR:
      HAL_UART_DMAStop(&huart5);
      gap_timer_stop();
      if (flag_usb_mounted)
      {
        f_close(&MyFile);
        f_mount(NULL, (TCHAR const*)USBHPath, 0);
        flag_usb_mounted = 0;
      }
      leds_all_off();
      led_set(GPIO_PIN_14, GPIO_PIN_SET);  /* LED rojo fijo */

      /* Reintentar si el usuario vuelve a insertar el pendrive */
      if (flag_usb_ready)
        app_state = STATE_MOUNT;
      break;
    }
  }
}

/**
  * @brief  TIM6: gap de 40ms detectado
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        flag_gap_detected = 1;
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
            flag_btn_unmount = 1;
    }
}
#endif
