/*
 * lcd.h
 *
 *  Created on: 9 ene. 2020
 *      Author: guillermo
 */

#ifndef INC_LCD_H_
#define INC_LCD_H_

// Textos de mensajes que se utilizan más de una vez, para no tener varias
// copias ocupando memoria, se los almacena en variables const.
extern const char _NULL_MSG[];
extern const char _MODULO_DETENIDO[];
extern const char _VGM_EIBE[];
extern const char _INICIANDO_SAS[];
extern const char _INVALIDA[];
extern const char _SAS_OK[];
extern const char _ERROR[];
extern const char _RETIRE_TARJETA[];
extern const char _MAQUINA_LIBRE[];
extern const char _MAQUINA_BLOQUEADA[];
extern const char _BLOQUEADA[];
extern const char _INGRESE_TARJETA[];
extern const char _INGRESE_BILLETE[];
extern const char _MAQUINA[];
extern const char _DISPONIBLE[];
extern const char _NO_PERMITIDO[];
extern const char _SIGA_JUGANDO[];
extern const char _DESCARGANDO_J[];
extern const char _DESCARGANDO_M[];
extern const char _DESCARGANDO_V[];
extern const char _ESPERE[];
extern const char _PROCESANDO[];
extern const char _LEYENDO_TARJETA[];
extern const char _NO_FUNCIONA[];
extern const char _PUERTA_ABIERTA[];
extern const char _DE_JUGADOR[];
extern const char _PRUEBE_OTRA_VEZ[];
extern const char _MAQUINA_JUGANDO[];
extern const char _BUENA_SUERTE[];
extern const char _TARJETA_INVALIDA[];
extern const char _PIN_INVALIDO[];
extern const char _INDETERMINADO[];
extern const char _ERROR_MONTO[];
extern const char _ERROR_TICKET[];
extern const char _RECHAZADO[];
extern const char _NO_TIENE_FONDOS[];
extern const char _DEBE_RECARGAR[];
extern const char _MODO_TEST[];
extern const char _IP_DUPLICADA[];
extern const char _IMPRESORA[];
extern const char _ERROR_EN[];
extern const char _CARGA_INVALIDA[];

void test_fsmc(void);
void prvTestTask(void *pvParameters);

void ks0108_init(void);
void ks0108_print(const char * txt1, const char * txt2);
void ks0108WriteCommand(uint8_t cmd, uint8_t chip);
void ks0108WriteData(uint8_t data);
void ks0108GotoXY(uint8_t x, uint8_t y);
void ks0108ClearScreen(void);
void ks0108Putchar(int x, int y, char c);
int ks0108Puts(int line, const char * s);
void ks0108Frame(void);

//extern volatile uint32_t msTicks;
//
//#define delayTicks(n)       for(msTicks=0;msTicks<n;)

// Display 2 x 16
//----------------------------------------
// Enviar un comando al display LCD
//----------------------------------------
void LCD_command(unsigned char cmd);

//----------------------------------------
// Enviar un caracter al display LCD
//----------------------------------------
void LCD_write(unsigned char data);

//----------------------------------------
// Borrar el display LCD
//----------------------------------------
void LCD_clear(void);

//---------------------------------------------------
// Posicionar el cursor del display LCD en el inicio
//---------------------------------------------------
void LCD_home(void);

//------------------------------------------------------
// Posicionar el cursor del display LCD en la 2da línea
//------------------------------------------------------
void LCD_line2(void);

//------------------------------------------------------
// Imprime un mensaje en el display LCD
//------------------------------------------------------
void LCD_print(const char * txt1, const char * txt2);
void LCD_print_raw(const char * txt1, const char * txt2);

#define LCD_print1(s) LCD_print(s,_NULL)
#define LCD_print2(s) LCD_print(_NULL,s)
//-----------------------------------------------
// Envía una cadena de caracteres al display LCD
//-----------------------------------------------
void LCD_puts( int line, const char * s );

//----------------------------------------
// Inicializa el display LCD
//----------------------------------------
void LCD_init(void);

/*
 * Presenta en el display el mensaje por defecto, pero
 * si la VGM está bloqueada muestra máquina bloqueada.
 */
void LCD_print_maquina_libre(void);

#define LCD_print_maquina_bloqueada() LCD_print_maquina_libre()

/*----------------------------------------------------------------------------
 *  Tarea que maneja el display LCD
 *----------------------------------------------------------------------------
 */
void LCD_task(void *pvParameters);

struct LcdMsg
{
  char txt1[16];
  char txt2[16];
};

#endif /* INC_LCD_H_ */
