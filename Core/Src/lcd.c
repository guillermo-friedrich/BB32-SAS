/*
 * lcd.c
 *
 *  Created on: 28 nov. 2019
 *      Author: guillermo
 */

//#include <string.h>
#include "cashless.h"
//#include "main.h"
//#include "lcd.h"
#include "fonts.h"
//#include "card.h"

static const char resetCause[8][17] = {"CAUSA DESCONOCIDA","BAJA TENSION","WINDOW WDG","INDEP. WATCHDOG","SOFTWARE","POWER ON","EXTERNAL PIN","BROWNOUT"};

// Textos de mensajes que se utilizan más de una vez, para no tener varias
// copias ocupando memoria, se los almacena en variables const.
const char _NULL_MSG[]="";
const char _MODULO_DETENIDO[]="MODULO DETENIDO";
const char _VGM_EIBE[]="VGM EIBE";
const char _INICIANDO_SAS[]="INICIANDO SAS";
const char _INVALIDA[]="INVALIDA";
const char _SAS_OK[]="SAS OK";
const char _ERROR[]="ERROR";
const char _RETIRE_TARJETA[]="RETIRE TARJETA";
const char _MAQUINA_LIBRE[]="MAQUINA LIBRE";
const char _MAQUINA_BLOQUEADA[]="MAQUINA BLOQ";
const char _BLOQUEADA[]="BLOQUEADA";
const char _INGRESE_TARJETA[]="INGRESE TARJETA";
const char _INGRESE_BILLETE[]="INGRESE BILLETE";
const char _MAQUINA[]="MAQUINA";
const char _DISPONIBLE[]="DISPONIBLE";
const char _NO_PERMITIDO[]="NO PERMITIDO";
const char _SIGA_JUGANDO[]="SIGA JUGANDO";
const char _DESCARGANDO_J[]="DESCARGANDO (J)";
const char _DESCARGANDO_M[]="DESCARGANDO (M)";
const char _DESCARGANDO_V[]="DESCARGANDO (V)";
const char _ESPERE[]="ESPERE...";
const char _PROCESANDO[]="PROCESANDO...";
const char _LEYENDO_TARJETA[]="LEYENDO TARJETA";
const char _NO_FUNCIONA[]="NO FUNCIONA";
const char _PUERTA_ABIERTA[]="PUERTA ABIERTA";
const char _DE_JUGADOR[]="DE JUGADOR.....";
const char _PRUEBE_OTRA_VEZ[]="PRUEBE OTRA VEZ";
const char _MAQUINA_JUGANDO[]="MAQUINA JUGANDO";
const char _BUENA_SUERTE[]="BUENA SUERTE";
const char _TARJETA_INVALIDA[]="TARJ. INVALIDA";
const char _PIN_INVALIDO[]="PIN INVALIDO";
const char _INDETERMINADO[]="INDETERMINADO";
const char _ERROR_MONTO[]="ERROR: MONTO";
const char _ERROR_TICKET[]="ERROR: TICKET";
const char _RECHAZADO[]="RECHAZADO";
const char _NO_TIENE_FONDOS[]="NO TIENE FONDOS";
const char _DEBE_RECARGAR[]="DEBE RECARGAR";
const char _MODO_TEST[]="MODO TEST";
const char _IP_DUPLICADA[]="IP DUPLICADA";
const char _IMPRESORA[]="IMPRESORA";
const char _ERROR_EN[]="ERROR EN";
const char _CARGA_INVALIDA[]="CARGA INVALIDA";


#define TEXTO_INICIAL_1 "OCEAN S.A."
#define TEXTO_INICIAL_2 "ONLINE-CASHLESS"

// Macros para manejar los pines a los display
#define LCD_RST_H()   HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET)
#define LCD_RST_L()   HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET)

#define LCD_ENA1_H()  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET)
#define LCD_ENA1_L()  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET)

#define LCD_ENA2_H()  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_SET)
#define LCD_ENA2_L()  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_RESET)

#define LCD_CS1_ON()  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_SET)
#define LCD_CS1_OFF() HAL_GPIO_WritePin(GPIOE, GPIO_PIN_7, GPIO_PIN_RESET)

#define LCD_CS2_ON()  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_SET)
#define LCD_CS2_OFF() HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET)

#define LCD_DI_D()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET)
#define LCD_DI_I()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET)

#define LCD_RS_R()    LCD_DI_D()
#define LCD_RS_S()    LCD_DI_I()

#define LCD_Write(val) (GPIOE->ODR=(GPIOE->ODR&0x00ff)|(val<<8))

//-------------------------------------------
// SECCION PARA EL DISPLAY KS0108 128x64
//-------------------------------------------

// Tipos
typedef struct s1
{
  uint8_t x;
  uint8_t y;
  uint8_t page;
} KS0108Coord;

// Variables estáticas
static KS0108Coord ks0108Coord;

#define toUpper(c)    ((c>='a' && c<='z')?c-'a'+'A':c)
#define underScore(c) ((c=='_')?'Z'+1:c);

// Comandos para manejo del display KS0108
#define KS0108_ON          0x3F
#define KS0108_OFF         0x3E
#define KS0108_SET_ADD     0x40
#define KS0108_SET_PAGE    0xB8
#define KS0108_DISP_START  0xC0

// Colores
#define BLACK           0xFF
#define WHITE           0x00

static void delayLoop(int n);

static void delayLoop(int n)
{
  int i=0;
  while(i<n)
    ++i;
}

//------------------------------------------------------
// Imprime un mensaje en el display de 128 x 64
//------------------------------------------------------
void ks0108_print(const char * txt1, const char * txt2)
{
  int line;
  ks0108ClearScreen();
  ks0108Frame();
  line = ks0108Puts(0, txt1);
  ks0108Puts(++line, txt2);
}

//----------------------------------------
// Inicializa el display gráfico
//----------------------------------------
void ks0108_init()
{
  LCD_ENA2_L();

  LCD_RST_L();    // Pone un pulso de reset
  delayLoop(100);
  LCD_RST_H();
  delayLoop(100);

  ks0108Coord.x = 0;
  ks0108Coord.y = 0;
  ks0108Coord.page = 0;

  ks0108WriteCommand(KS0108_ON, 1);          // power on
  ks0108WriteCommand(KS0108_ON, 2);

  ks0108WriteCommand(KS0108_DISP_START, 1);  // display start line = 0
  ks0108WriteCommand(KS0108_DISP_START, 2);

  ks0108ClearScreen();
  ks0108Frame();
  ks0108Puts(0, TEXTO_INICIAL_1);
  ks0108Puts(1, TEXTO_INICIAL_2);
}

//------------------------------------------------
// Escribe un comando en uno de los controladores
//------------------------------------------------
void ks0108WriteCommand(uint8_t cmd, uint8_t chip)
{
  if (chip == 1)
  {
    LCD_CS2_OFF();
    LCD_CS1_ON();
  } else
  {
    LCD_CS1_OFF();
    LCD_CS2_ON();
  }

  LCD_DI_I();
  LCD_Write(cmd);
  delayLoop(100);

  LCD_ENA2_H();
  delayLoop(10);
  LCD_ENA2_L();
  delayLoop(10);
}

//------------------------------------------------
// Escribe un dato
//------------------------------------------------
void ks0108WriteData(uint8_t data)
{
  if (ks0108Coord.x < 64)
  {
    LCD_CS2_OFF();
    LCD_CS1_ON();
  } else
  {
    LCD_CS1_OFF();
    LCD_CS2_ON();
  }

  LCD_DI_D();
  LCD_Write(data);
  delayLoop(100);

  LCD_ENA2_H();
  delayLoop(10);
  LCD_ENA2_L();
  delayLoop(10);

  if (++ks0108Coord.x == 64)
    ks0108GotoXY(ks0108Coord.x, ks0108Coord.y);
  else if (ks0108Coord.x >= 128)
    ks0108GotoXY(0, ks0108Coord.y + 8);
}

//------------------------------------------------
//
//------------------------------------------------
void ks0108GotoXY(uint8_t x, uint8_t y)
{
  uint8_t chip = 1, cmd;

  x = x % 128;            // asegura que las coordenadas sean válidas
  y = y % 64;

  ks0108Coord.x = x;      // almacena las nuevas coordenadas
  ks0108Coord.y = y;
  ks0108Coord.page = y / 8;

  if (x >= 64)
  {                       // selecciona el chip que corresponda
    x -= 64;
    chip = 2;
  }
  cmd = KS0108_SET_ADD | x;

  ks0108WriteCommand(cmd, chip);               // set x address on active chip

  cmd = KS0108_SET_PAGE | ks0108Coord.page;       // set y address on both chips

  ks0108WriteCommand(cmd, 1);
  ks0108WriteCommand(cmd, 2);
}

//------------------------------------------------
//
//------------------------------------------------
void ks0108ClearScreen()
{
  int i, j, k;
  for (k = 0; k < 128; k += 64)
  {
    for (j = 0; j < 64; j += 8)
    {
      ks0108GotoXY(k, j);
      for (i = 0; i < 64; ++i)
        ks0108WriteData(0x00);
    }
  }
  ks0108GotoXY(0, 0);
}

//------------------------------------------------
//
//------------------------------------------------
void ks0108Putchar(int x, int y, char c)
{
  static int i, j;

  // Como usa un subconjunto de ASCII, convierte a mayúsculas
  // y reubica el '_'
  c = toUpper(c);
  c = underScore(c);

  // Font 8x14
  j = 16 * (c - 0x20);
  ks0108GotoXY(x, y);
  for (i = 0; i < 8; ++i)
    ks0108WriteData(Font8x14[j + i]);
  ks0108GotoXY(x, y + 8);
  for (i = 8; i < 16; ++i)
    ks0108WriteData(Font8x14[j + i]);
}

//------------------------------------------------
//
//------------------------------------------------
int ks0108Puts(int line, const char * s)
{
  int i, j, n, m, w;

  // Saltea los espacios iniciales
  while (*s == ' ')
    ++s;

  w = 16;
  m = 8;

  n = strlen(s);
  if (n && n <= w)
  {
    // el texto cabe completo en un renglón
    for (i = 0; s[i] && i < 16; ++i)
    {
      // El texto con fonts de 8 píxeles de ancho va centrado
      ks0108Putchar(6 - 2 * (n % 2) + 8 * (i + ((15 - n) / 2)), 16 + line * 22,
          s[i]);
    }
  } else if (n > w)
  {
    // el texto debe ir en dos renglones
    // busca un espacio anterior a los 10 caracteres,
    for (n = n - 1; n && (n > w || s[n] != ' '); --n)
      ;

    if (s[n] == ' ')
    {
      // el texto anterior al espacio va en la línea inicial
      for (i = 0; i < n; ++i)
      {
        ks0108Putchar(m * i, line * 16, s[i]);
      }
      ++line;
      // el texto que sigue al espacio va en la línea siguiente
      for (i = n + 1, j = 0; s[i] && j < w; ++i, ++j)
      {
        ks0108Putchar(m * j, line * 16, s[i]);
      }
    } else
    {
      // no hay espacio --> escribe la palabra hasta el límite del renglón
      for (i = 0; s[i] && i < w; ++i)
      {
        ks0108Putchar(m * i, line * 16, s[i]);
      }
    }
  }
  return line;
}

//--------------------------------------------------
// Presenta un recuadro en el display de 128x64
//--------------------------------------------------
void ks0108Frame()
{
  int i;
  unsigned char c;

  ks0108GotoXY(0, 0);

  for (i = 0; i < 1024; ++i)
  {
    if (i == 0 || i == 127)
      c = 0xfc;
    else if (i == 1 || i == 126)
      c = 0x02;
    else if (i < 127)
      c = 0x01;
    else if (i == 896 || i == 1023)
      c = 0x3f;
    else if (i == 897 || i == 1022)
      c = 0x40;
    else if (i > 897 && i < 1022)
      c = 0x80;
    else if (i % 128 == 0 || i % 128 == 127)
      c = 0xff;
    else
      c = 0x00;
    ks0108WriteData(c);
  }
}

//----------------------------------------------
// Sección para el LCD de 2x16
//----------------------------------------------

//----------------------------------------
// Enviar un comando al display LCD
//----------------------------------------
void LCD_command(unsigned char cmd)
{
  LCD_RS_S();
  LCD_Write(cmd);
  LCD_ENA1_H();
  HAL_Delay(2);
  LCD_ENA1_L();
  HAL_Delay(2);
  LCD_RS_R();
  HAL_Delay(2);
}

//----------------------------------------
// Enviar un caracter al display LCD
//----------------------------------------
void LCD_write(unsigned char data)
{
  LCD_RS_R();
  LCD_Write(data);
  LCD_ENA1_H();
  HAL_Delay(2);
  LCD_ENA1_L();
  HAL_Delay(2);
}

//----------------------------------------
// Borrar el display LCD
//----------------------------------------
void LCD_clear()
{
  LCD_command(0x01);

  HAL_Delay(3);    // > 1.65 ms
}

//---------------------------------------------------
// Posicionar el cursor del display LCD en el inicio
//---------------------------------------------------
void LCD_home()
{
  LCD_command(0x02);

  HAL_Delay(3);    // > 1.65 ms
}

//------------------------------------------------------
// Posicionar el cursor del display LCD en la 2da línea
//------------------------------------------------------
void LCD_line2()
{
  LCD_command(0x0C0);
}

//---------------------------------------------------------------------------
// Imprime un mensaje en el display.
// Esta función usa un semáforo para que si se la invoca desde varias tareas
// al mismo tiempo, no pueda ejecutarse en forma reentrante.
//---------------------------------------------------------------------------
void LCD_print(const char * txt1, const char * txt2)
{
  // Primero imprime en el display de 2 x 16
  if(txt1)
    LCD_puts(1, txt1);
  if(txt2)
    LCD_puts(2, txt2);

  // Luego imprime en el display gráfico
  ks0108_print( txt1, txt2 );

  HAL_Delay(1000);
}

/*
 * Imprime directamente en ambos displays. Usar sólo al grabar la flash, porque están
 * deshabilitadas las interrupciones.
 */
void LCD_print_raw(const char *txt1, const char *txt2)
{
  // Primero imprime en el display de 2 x 16
  if(txt1)
    LCD_puts(1, txt1);
  if(txt2)
    LCD_puts(2, txt2);

  // Luego imprime en el display gráfico
  ks0108_print( txt1, txt2 );

  HAL_Delay(1000);
}

//-----------------------------------------------
// Envía una cadena de caracteres al display LCD
//-----------------------------------------------
void LCD_puts(int line, const char * s)
{
  int i, j;

  if (line == 1)
    LCD_home();
  else
    LCD_line2();

  for (i = strlen(s), j = 0; j < ((16 - i) / 2); ++j)
    LCD_write(' ');

  for (i = 0; j < 16 && s[i] && s[i] != 13; ++i, ++j)
    LCD_write(s[i]);

  while (j++ < 16)
    LCD_write(' ');      // completa con espacios hasta el final del renglón
}

//----------------------------------------
// Inicializa el display LCD
//----------------------------------------
void LCD_init()
{
  // Primero inicializa el display de 2 x 16

  // Espera luego del encendido, antes de ejecutar
  // la inicialización del display, para dar tiempo
  // a que el display y el controlador completen
  // el reset.

  HAL_Delay(100);      // 100ms delay after power-up

  LCD_command(0x30);    // Function set
  HAL_Delay(50);       // Delay more than 39us

  LCD_command(0x30);    // Function set
  HAL_Delay(50);       // Delay more than 39us

  LCD_command(0x30);    // Function set
  HAL_Delay(50);       // Delay more than 39us

  LCD_command(0x3C);    // 1/16 duty cycle.  5x11 font
  HAL_Delay(50);       // Delay more than 39us

  LCD_command(0x0C);    // Disp on, Blink off, Cursor off
  HAL_Delay(50);       // Delay more than 39us

  LCD_command(0x01);    // Clear/Home
  HAL_Delay(3);        // Delay more than 1.53ms

  LCD_command(0x06);    // Increment right
  HAL_Delay(3);        // Delay more than 1.53ms

  // Luego inicializa el display gráfico de 128 x 64

  ks0108_init();

  // Pone un mensaje inicial

  LCD_print(TEXTO_INICIAL_1, TEXTO_INICIAL_2);
  LCD_print("REINICIO POR",resetCause[GetResetCause()]);
}

#if 1
/*
 * Presenta en el display el mensaje por defecto, pero
 * si la VGM está bloqueada muestra máquina bloqueada.
 */
void LCD_print_maquina_libre(void)
{
  if( flag.lockVGM )
    LCD_print( _MAQUINA, _BLOQUEADA );
  else
  {
    if( getCardStatus() )
    {
       if( currentCredits )
         LCD_print( _MAQUINA_JUGANDO, _BUENA_SUERTE );
       else
         LCD_print1( _RETIRE_TARJETA );
    }
    else
    {
      if( !flag.vgmAlive )
        LCD_print( "VGM NO RESPONDE", "LLAMAR TECNICO" );
      else
        LCD_print( _MAQUINA_LIBRE, _INGRESE_TARJETA );
    }
  }
}

#define LCD_print_maquina_bloqueada() LCD_print_maquina_libre()
#endif
