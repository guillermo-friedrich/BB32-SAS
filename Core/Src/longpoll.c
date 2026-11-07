/*
 * longpoll.c
 *
 *  Created on: 30 nov. 2019
 *      Author: guillermo
 */
#if 0
#include "cashless.h"

uint32_t polling( int np )
{
  return 0;
}

void sendMeters()
{

}

void OnDemandLongPoll( ip_addr_t * _addr, uint16_t _port, struct udp_pcb *_upcb, struct pbuf * _p )
{

}

#else
#include "cashless.h"

// tabla de valores de transmisión (unidad de denominación). En el LP1F viene un código
// con el que se accede a esta tabla para encontrar el valor en centavos
const uint32_t denominacion[]= {0,1,5,10,25,50,100,500,1000,2000,10000,20,200,250,2500,5000,20000,25000,
                                    50000,100000,200000,250000,500000,2,3,15,40};LongPollMsgType longPollBuffer;

/*
 * Estructura para almacenar los comandos de los long polls, la longitud de sus respuestas
 * y el offset para ubicar las respuestas dentro del buffer
 */

typedef struct
{
	const uint8_t lpCommand;     // código del comando
	const uint8_t * parameters;  // puntero a los parámetros del comando o NULL si es sólo lpCommand
} LongPollType;

typedef struct
{
	uint8_t code;       // código del LP o código del LP extendido
	uint8_t offset;     // offset al primer byte de interés en la respuesta al LP (0 si la
                      // posición de los datos viene indicada en la respuesta (LP 2F, 6F o AF).
                      // Este valor en cero también indica que hay que copiar la longitud en
                      // la primera posición del destino.
	uint8_t nBytes;     // cantidad de bytes a copiar de la respuesta al destino (si es LP 2F.
                      // Si es LP 6F la está indicada en la respuesta al LP).
	uint8_t * destino;  // puntero a donde se deben almacenar los datos
} RespLongPollType;

/// Parámetros del LP A0
const uint8_t lpa0[] = {0x00,0x00};  //Solicita features de la máquina

// Parámetros de los LP 6F
const uint8_t lp6f_1[] = {18,0x00,0x00,0x40,0,0x41,0,0x42,0,0x43,0,0x44,0,0x46,0,0x47,0,0x48,0};  //Billetes de $1 a $200
const uint8_t lp6f_2[] = {18,0x00,0x00,0x4a,0,0x4b,0,0x4c,0,0x4e,0,0x4f,0,0x50,0,0x52,0,0x53,0};  //Billetes de $500 a $100.000
const uint8_t lp6f_3[] = {22,0x00,0x00,0x0c,0x00,0x00,0x00,0x01,0x00,0x02,0x00,0x03,0x00,0x04,0x00,0x24,0x00,0x17,0x00,0x18,0x00,0x0b,0x00};  //Current crédits, etc.
const uint8_t lp6f_4[] = {14,0x00,0x00,0x0d,0x00,0x0e,0x00,0x20,0x00,0x21,0x00,0x1d,0x00,0x1e,0x00};  //Total cashable tickets in (ctvs), etc.

const uint8_t lp2f[] = {6,0x00,0x00,0x0d,0x0e,0x17,0x18};  //Total EFT in, total EFT out, total ticket in, total ticket out

// Comandos para los Long Polls usados para consultar a la máquina
// 1er campo: código del LP
// 2do campo: puntero a los parámetros del LP (o nulo si no lleva parámetros)

const LongPollType longPoll[] = { {0xa0, lpa0}, {0x1f, 0}, {0x54, 0}, {0x1c, 0},
		{0x6f, lp6f_1}, {0x6f, lp6f_2},     // Estos se ejecutan si soporta LP extendidos
		{0x6f, lp6f_3}, {0x6f, lp6f_4} };   // ídem

const LongPollType legacyLongPoll[] = { {0x31, 0}, {0x32, 0}, {0x33, 0}, {0x34, 0}, {0x35, 0}, {0x36, 0}, {0x37, 0}, {0x3a, 0},
		{0x38, 0}, {0x39, 0}, {0x3c, 0}, {0x3f, 0}, {0x40, 0}, {0x41, 0}, {0x43, 0}, {0x44, 0},
		{0x1a, 0}, {0x1c, 0}, {0x1d, 0}, {0x2d, 0}, {0x10, 0}, {0x46, 0}, {0x2f, lp2f} };

// Tabla que define el procesamiento de las respuestas a los LP originales
static const RespLongPollType legacyLongPollResponse[] = {
		{0x31, 2, 4, longPollBuffer.lp31},   // Billetes de $1 a $200
		{0x32, 2, 4, longPollBuffer.lp32},
		{0x33, 2, 4, longPollBuffer.lp33},
		{0x34, 2, 4, longPollBuffer.lp34},
		{0x35, 2, 4, longPollBuffer.lp35},
		{0x36, 2, 4, longPollBuffer.lp36},
		{0x37, 2, 4, longPollBuffer.lp37},
		{0x3a, 2, 4, longPollBuffer.lp3a},

		{0x38, 2, 4, longPollBuffer.lp38},   // Billetes de $500 a $100.000
		{0x39, 2, 4, longPollBuffer.lp39},
		{0x3c, 2, 4, longPollBuffer.lp3c},
		{0x3f, 2, 4, longPollBuffer.lp3f},
		{0x40, 2, 4, longPollBuffer.lp40},
		{0x41, 2, 4, longPollBuffer.lp41},
		{0x43, 2, 4, longPollBuffer.lp43},
		{0x44, 2, 4, longPollBuffer.lp44},

		{0x1a, 2, 4,  longPollBuffer.lp6f0c},  // Current credits
		{0x1c, 2, 4,  longPollBuffer.lp6f00},  // Total coin in
		{0x1c, 6, 4,  longPollBuffer.lp6f01},  // Total coin out
		{0x1c, 14, 4, longPollBuffer.lp6f02},  // Total jackpot
		{0x2d, 4, 4,  longPollBuffer.lp6f03},  // Total hand paid cancelled credits
		{0x10, 2, 4,  longPollBuffer.lp6f04},  // Total cancelled credits
		{0x1c, 10, 4, longPollBuffer.lp6f24},  // Total drop
		{0x46, 2, 4,  longPollBuffer.lp6f0b},  // Total credits from bills accepted

    /* Agregado para las máquinas IGT viejas, que no mueven el drop del LP1C. Entonces
     * se hace un LP1D (exclusivo de IGT) y se le extraen los últimos cuatro bytes, con
     * los que se reemplaza el drop del LP1C y el Total Electronics Transfers to VGM del LP2F,
     * y los cuatro anteúltimos bytes con los que se reemplaza el Total Electronics
     * Transfer to Host del LP2F */
		{0x1d, 14, 4, longPollBuffer.lp6f24},  // Emulación de Drop
		{0x1d, 14, 4, longPollBuffer.lp6f17},  // Emulación de Total Electronic Transfer to VGM
		{0x1d, 10, 4, longPollBuffer.lp6f18}   // Emulación de Total Electronic Transfer to Host
};

// Tabla que define el procesamiento de las respuestas a los LP 2f
static const RespLongPollType longPoll2fResponse[] = {
		/* Estos son para procesar el LP 2F que se hace para las máquinas
		 * que no aceptan contadores extendidos */
		{0x0d, 0, 5, longPollBuffer.lp6f0d},  // Total cashable ticket in (ctvs)
		{0x0e, 0, 5, longPollBuffer.lp6f0e},  // Total cashable ticket out (ctvs)
		{0x17, 0, 4, longPollBuffer.lp6f17},  // Total EFT to VGM (credits)
		{0x18, 0, 4, longPollBuffer.lp6f18}   // Total EFT to host (credits)
};
#define TOTAL_LEGACY_POLLS (sizeof(legacyLongPoll)/sizeof(LongPollType))  // Cantidad total de comandos de Legacy Long Poll a efectuar
#define TOTAL_LEGACY_LP_RESPONSES (sizeof(legacyLongPollResponse)/sizeof(RespLongPollType))  // Cantidad total de respuestas de Legacy Long Poll
#define TOTAL_LP_2F_RESPONSES (sizeof(longPoll2fResponse)/sizeof(RespLongPollType))          // Cantidad total de respuestas de LP 2F

#define isLegacyBillMeter(code) ((code>=0x31 && code<=0x45)?1:0)  // Macro para determinar si un legacy LP corresponde a un billete

// Indices a determinados LP que se pueden solicitar específicamente
#define NPOLL_CURRENT_CREDITS                  16
#define NPOLL_COIN_IN                          17
#define NPOLL_EMULA_DROP                       18
#define NPOLL_TOTAL_HANDPAID_CANCELLED_CREDITS 19
#define NPOLL_TOTAL_TICKETS_IN_OUT             22

// Tabla que define el procesamiento de las respuestas a los LP normales
static const RespLongPollType longPollResponse[] = {
		{0xa0, 4,  3, longPollBuffer.lpa0},
		{0x1c, 18,16, longPollBuffer.lp1c},
		{0x1f, 7,  1, longPollBuffer.lp1f},
		{0x1f, 18, 4, longPollBuffer.lp1f+1},
    {0x1f, 2,  2, (uint8_t *)vgmID},
		{0x54, 3,  3, longPollBuffer.lp54},
};
// Tabla que define el procesamiento de las respuestas a los LP 2F y 6F
static const RespLongPollType longPollResponse6f[] = {
		{0x40, 0, 4, longPollBuffer.lp31},   // Billetes de $1 a $200
		{0x41, 0, 4, longPollBuffer.lp32},
		{0x42, 0, 4, longPollBuffer.lp33},
		{0x43, 0, 4, longPollBuffer.lp34},
		{0x44, 0, 4, longPollBuffer.lp35},
		{0x46, 0, 4, longPollBuffer.lp36},
		{0x47, 0, 4, longPollBuffer.lp37},
		{0x48, 0, 4, longPollBuffer.lp3a},

		{0x4a, 0, 4, longPollBuffer.lp38},   // Billetes de $500 a $100.000
		{0x4b, 0, 4, longPollBuffer.lp39},
		{0x4c, 0, 4, longPollBuffer.lp3c},
		{0x4e, 0, 4, longPollBuffer.lp3f},
		{0x4f, 0, 4, longPollBuffer.lp40},
		{0x50, 0, 4, longPollBuffer.lp41},
		{0x52, 0, 4, longPollBuffer.lp43},
		{0x53, 0, 4, longPollBuffer.lp44},

		{0x0c, 0, 4, longPollBuffer.lp6f0c},  // Current credits
		{0x00, 0, 4, longPollBuffer.lp6f00},  // Total coin in
		{0x01, 0, 4, longPollBuffer.lp6f01},  // Total coin out
		{0x02, 0, 4, longPollBuffer.lp6f02},  // Total jackpot
		{0x03, 0, 4, longPollBuffer.lp6f03},  // Total hand paid cancelled credits
		{0x04, 0, 4, longPollBuffer.lp6f04},  // Total cancelled credits
		{0x24, 0, 4, longPollBuffer.lp6f24},  // Total drop
		{0x17, 0, 4, longPollBuffer.lp6f17},  // Total EFT to VGM
		{0x18, 0, 4, longPollBuffer.lp6f18},  // Total EFT to host
		{0x0b, 0, 4, longPollBuffer.lp6f0b},  // Total credits from bills accepted

		{0x0d, 0, 5, longPollBuffer.lp6f0d},  // Total cashable tickets in (ctvs)
		{0x0e, 0, 5, longPollBuffer.lp6f0e},  // Total cashable tickets out (ctvs)
		{0x20, 0, 4, longPollBuffer.lp6f20},  // Total attendant paid progressive win (credits)
		{0x21, 0, 4, longPollBuffer.lp6f21},  // Total attendant paid external bonus win (credits)
		{0x1d, 0, 4, longPollBuffer.lp6f1d},  // Total machine paid progressive win (credits)
		{0x1e, 0, 4, longPollBuffer.lp6f1e}   // Total machine paid external bonus win (credits)
};

#define TOTAL_POLLS (sizeof(longPoll)/sizeof(LongPollType))                        // Cantidad total de comandos de Long Poll a efectuar
#define TOTAL_LP_RESPONSES (sizeof(longPollResponse)/sizeof(RespLongPollType))     // Cantidad total de respuestas de Long Poll
#define TOTAL_LP6F_RESPONSES (sizeof(longPollResponse6f)/sizeof(RespLongPollType)) // Cantidad total de respuestas de Long Poll 2f y 6f

#define isExtendedBillMeter(code) ((code>=0x40 && code<=0x57)?1:0)  // Macro para determinar si un extended LP corresponde a un billete

#define NEXT_CICLIC_POLL -1                // Parámetro para que polling() haga el próximo LP según la secuencia cíclica

#define NPOLL_FEATURES 0                   // Indice al comando del LP A0
#define NPOLL_DENOMINATION 1               // Indice al comando del LP 1F
#define NPOLL_EXTENDED 4                   // Indice al primer LP extendido

#define NPOLL_EXT_CREDITS  6                    // Indice al comando del LP 6F que obtiene current credits
#define NPOLL_EXT_COIN_IN  6                    // Indice al comando del LP 6F que obtiene el total coin in y otros
#define NPOLL_EXT_HANDPAID_CANCELLED_CREDITS 6  // Indice al comando del LP 6F que obtiene total hand paid cancelled credits
#define NPOLL_EXT_TOTAL_TICKETS_IN_OUT       7  // Indice al comando del LP 6F que obtiene total tickets in y out

LongPollMsgType longPollBuffer;

static int nPoll;

/*-------------------------
 * Funciones
 *-------------------------
 */
#if 0
/**
 * @brief FreeRTOS-Task "SEND_METERS"
 *        Envía mensaje de contadores al servidor online
 */
void SendMeters_task(void *pvParameters)
{
	clearLongPollBuffer();

	while(1)
	{
    // Solicita acceso exclusivo al buffer de mensaje de Long Poll
    if( xSemaphoreTake(xMutexLongPoll, portMAX_DELAY) == pdTRUE )
    {
      // Solicita acceso exclusivo al uso de Ethernet
      if( xSemaphoreTake(xMutexEth, portMAX_DELAY) == pdTRUE )
      {
        // Prepara y envía el mensaje con los valores actuales de los contadores
        sendMeters();

        // Libera el uso de Ethernet
        xSemaphoreGive(xMutexEth);
      }
      // Libera el uso del buffer de mensaje de Long Poll
      xSemaphoreGive(xMutexLongPoll);
    }
		//Delay de 30 s hasta el próximo envío de Contadores
		vTaskDelay( _30seg );
	}
}

/**
 * @brief FreeRTOS-Task "SEND_METERS"
 *        Envía mensaje de contadores al controlador del progresivo
 */
void SendMetersProg_task(void *pvParameters)
{
	clearLongPollBuffer();

	while(1)
	{
		if ( xSemaphoreTake( xSemaphoreSendMetersProg, portMAX_DELAY ) == pdTRUE )
		{
      // Solicita acceso exclusivo al buffer de mensaje de Long Poll
      if( xSemaphoreTake(xMutexLongPoll, portMAX_DELAY) == pdTRUE )
      {
        // Solicita acceso exclusivo al uso de Ethernet
        if( xSemaphoreTake(xMutexEth, portMAX_DELAY) == pdTRUE )
        {
          // Prepara y envía el mensaje con los valores actuales de los contadores
          sendMeters();

          // Libera el uso de Ethernet
          xSemaphoreGive(xMutexEth);
        }
        // Libera el uso del buffer de mensaje de Long Poll
        xSemaphoreGive(xMutexLongPoll);
      }
		}
	}
}
#endif

/**
 * @brief  Envía un mensaje de contadores al servidor online o al controlador del
 *         progresivo, dependiendo del estado del flag sendCoinInProg
 * @param  None
 * @return None
 */
void sendMeters()
{
	int i;

	longPollBuffer.msgID[0]='L';
	longPollBuffer.msgID[1]='P';
	for( i=0; i<6; ++i )
		longPollBuffer.mac[i] = MyMAC[i];
	longPollBuffer.hash = rand();
	longPollBuffer.crc = CRC16( ( uint8_t * )&longPollBuffer, sizeof( LongPollMsgType )-2, 0 );

	cifrar( ( uint8_t * )&longPollBuffer, sizeof( LongPollMsgType ) );

	if( flag.sendCoinIn )
	{
    sendUDP( (uint8_t *) &longPollBuffer, sizeof(LongPollMsgType), upcb_cli, &addr_serv_prog, SERV_PORT_MISTERY );

		flag.sendCoinIn = 0;
	}
	else
	{
		sendUDP( (uint8_t *) &longPollBuffer, sizeof(LongPollMsgType), upcb_cli, &addr_serv_online, SERV_PORT_CONTADORES );
	}

	decifrar( ( uint8_t * )&longPollBuffer, sizeof( LongPollMsgType ) );
}

/**
 * @brief  Borra el buffer donde almacena las respuestas a los Long Polls
 * @param  None
 * @retval None
 */
#define p ((uint8_t *)&longPollBuffer)

void clearLongPollBuffer( void )
{
	int i;

	longPollBuffer.msgID[0]='L';
	longPollBuffer.msgID[1]='P';

	for( i=2; i<sizeof( LongPollMsgType ); ++i )
		p[i] = 0xff;

	for( i=0; i<3; ++i )
		longPollBuffer.lpa0[i] = 0;

	for( i=0; i<5; ++i )
		longPollBuffer.lp1f[i] = 0;

	for( i=0; i<4; ++i )
		longPollBuffer.lp54[i] = 0;

	for( i=0; i<16; ++i )
		longPollBuffer.lp6f0c[10*i] = 0; // Pone en cero la longitud de los contadores extendidos
}

#undef p

//----------------------------------------------------------------
// Borra un Long Poll en el buffer de mensaje de LP
//----------------------------------------------------------------

static void clearLongPoll( uint8_t code, const uint8_t * parameters )
{
	int i, j, k=0, n;
	uint8_t * p, c=0xff;

	switch(code)
	{
	case 0x6f:
		for( n=3; n<parameters[0]; n+=2 )
		{
			for(i=0; i<TOTAL_LP6F_RESPONSES; ++i)
			{
				if( parameters[n] == longPollResponse6f[i].code )
				{
					if( isExtendedBillMeter( parameters[n] ) )
						k = 4;
					else
					{
						k = 10;
						longPollResponse6f[i].destino[0] = 0;  // longitud cero
					}
					for(j=1; j<k; ++j, ++p)
						longPollResponse6f[i].destino[j] = c;
					break;
				}
			}
		}
		break;
	default:
		if( code==0x54 || code==0x1f || code==0xa0 )
			c = 0;

		for(i=0; i<TOTAL_LP_RESPONSES; ++i)
		{
			if( code == longPollResponse[i].code )
			{
				for(j=0; j<longPollResponse[i].nBytes; ++j)
					longPollResponse[i].destino[j] = c;
			}
		}

		if( !flag.extendedLongPolls )
		{
			for(i=0; i<TOTAL_LEGACY_LP_RESPONSES; ++i)
			{
				if( code == legacyLongPollResponse[i].code )
				{
					if( isLegacyBillMeter(code) )
						k = 4;
					else
						k = 10;

					for(j=0; j<k; ++j)
						legacyLongPollResponse[i].destino[j] = c;
				}
			}
		}
		break;
	}
}

/*
 ** Retorna 1 si una cadena de bytes contiene todos dígitos BCD, 0 si alguno no lo es.
 */
static int checkBCD(uint8_t *p, int n)
{
	int i;
	uint8_t c;

	for(i=0; i<n; ++i)
	{
		c = p[i] & 0x0f;
		if( !(c>=0 && c<=9) )
			return 0;
		c = p[i] >> 4;
		if( !(c>=0 && c<=9) )
			return 0;
	}
	return 1;
}

/*------------------------------------------------------------------------------------
 * Realiza el ciclo de Long Polls para máquinas que no soportan LP extendidos.
 * Parámetro: np --> si < 0 realiza el LP que corresponda según la secuencia cíclica
 *                   si >= 0 realiza un LP en particular a pedido
 * Retorna rx_buflen --> 0 indica que no hubo respuesta.
 *------------------------------------------------------------------------------------*/

static uint8_t legacyPolling( int np )
{
	static int nLegacyPoll=0;
	uint8_t ret = 0;
	int i=0, j, k, n=nLegacyPoll, emula_drop=0;

	if( np >= 0 )
	{
		// Se pidió un LP fuera de la secuencia cíclica
		n = np;
	}

	do
	{
		tx_buffer[0]=vgmAddr;
		tx_buffer[1]=legacyLongPoll[n].lpCommand;

		if( legacyLongPoll[n].parameters )
		{
			// El LP 2F lleva parámetros y el 1er byte es la longitud
			tx_buflen = 5 + legacyLongPoll[n].parameters[0];

			for(i=0; i<(tx_buflen-2); ++i)
				tx_buffer[i+2] = legacyLongPoll[n].parameters[i];

			i=CRC16(tx_buffer,tx_buflen-2,0);

			tx_buffer[tx_buflen-2] = i & 0x00ff;
			tx_buffer[tx_buflen-1] = ( i >> 8 ) & 0x00ff;
		}
		else
			tx_buflen = 2;

		// Envía el LP a la máquina
		UART0_tx( tx_buffer, tx_buflen );  // comando y eventuales parámetros

		// Espera la respuesta
		if( !UART0_rx() )
		{
			// La VGM no respondió --> almacena 00 o FF en el campo correspondiente del mensaje
			clearLongPoll(legacyLongPoll[n].lpCommand, legacyLongPoll[n].parameters);
		}
		else
		{
			/* Verifica que la respuesta recibida sea correcta, y en tal caso la almacena
         en el buffer del mensaje de contadores */

			if( CRC16( rx_buffer,rx_buflen,0 )==0 && rx_buffer[0]==vgmAddr && rx_buffer[1]==legacyLongPoll[n].lpCommand )
			{
				// Procesa la respuesta según el código del comando del LP
				if( rx_buffer[1] == 0x2f )
				{
					/* Es un LP 2F --> procesa la respuesta según los códigos que hay en su contenido.
             Por las dudas verifica que la longitud recibida coincida con la longitud indicada
             en la respuesta. */
					if( rx_buflen == (rx_buffer[2]+5) )
					{
						ret = rx_buflen;
						k = 5;
						while( k < (rx_buffer[2]+3) )
						{
							for(i=0; i<TOTAL_LP_2F_RESPONSES; ++i)
							{
								if( longPoll2fResponse[i].code == rx_buffer[k] )
								{
									longPoll2fResponse[i].destino[0] = longPoll2fResponse[i].nBytes;
									for(j=0; j<longPoll2fResponse[i].nBytes; ++j)
										longPoll2fResponse[i].destino[j+1] = rx_buffer[k+j+1];
									k += (longPoll2fResponse[i].nBytes + 1);
									break;
								}
							}
							if( i >= TOTAL_LP_2F_RESPONSES )
							{
								/* Recorrió toda la lista de respuestas esperadas y no encontró el código buscado -->
                   --> aborta el proceso porque no sabe cuántos bytes tendría que saltar.
                   En realidad esta comprobación está puesta por las dudas, porque no debería
                   aparecer ningún código de contador que no se haya solicitado. */
								ret = 0;
								break;  // Sale del while
							}
						}
					}
				}
				else
				{
					// Es la respuesta a un LP simple.
					for(i=0; i<TOTAL_LEGACY_LP_RESPONSES; ++i)
					{
						if( isLegacyBillMeter(rx_buffer[1]) && !checkBCD(rx_buffer+2, 4) )
						{
							// Descarta contador de billetes que no viene en BCD
							break;
						}

						if( rx_buffer[1] == legacyLongPollResponse[i].code )
						{
							if( isLegacyBillMeter(rx_buffer[1]) )
							{
								k=0;
							}
							else
							{
								k=1;
								legacyLongPollResponse[i].destino[0] = legacyLongPollResponse[i].nBytes;
							}
							for(j=0; j<legacyLongPollResponse[i].nBytes; ++j)
								legacyLongPollResponse[i].destino[j+k] = rx_buffer[legacyLongPollResponse[i].offset + j];

							// Si es el lugar de un contador extendido, rellena hasta el final con FF
							if(k)
								for( ; j<9; ++j)
									legacyLongPollResponse[i].destino[j+k] = 0xff;
						}
					}
				}
				ret = rx_buflen;
			}
		}

		if( n == NPOLL_CURRENT_CREDITS )
		{
			// Actualiza Current Credits
			if( !flag.waitingClientCard && longPollBuffer.lp6f0c[0] <= 9 )
				currentCredits = bcd2long( longPollBuffer.lp6f0c + 1, 2 * longPollBuffer.lp6f0c[0] );
		}

		if( n == NPOLL_COIN_IN && (features.cashless & DROP_LP1D) )
		{
			// Si se debe emular el drop, luego del LP 1C se hace un LP 1D
			emula_drop = 1;
			delay( pollingPeriod );
			n = NPOLL_EMULA_DROP;
		}else{
			emula_drop = 0;
		}
	}while(emula_drop);

	if( np < 0 )
	{
		// Pasa al siguiente comando para la próxima vuelta
		nLegacyPoll = ( nLegacyPoll + 1 ) % TOTAL_LEGACY_POLLS;

		if( nLegacyPoll == NPOLL_EMULA_DROP && !(features.cashless & DROP_LP1D) )
		{
			// Si no corresponde efectuarlo, saltea LP 1D
			nLegacyPoll = ( nLegacyPoll + 1 ) % TOTAL_LEGACY_POLLS;
		}

		if(nLegacyPoll == 0)
		{
			// Hizo una vuelta completa de Legacy LP, vuelve a comenzar la vuelta general
			nPoll = 0;
			// Completó una vuelta de pollings --> habilita el envío de mensajes de contadores al servidor.
			flag.sendLongPollEnabled = 1;
		}
	}
	return ret;
}

/*
 ** Setea fecha y hora de la VGM. Lo hace por única vez al iniciar el ciclo de polling
 */
static uint8_t setVGMDateTime()
{
	int i;
	flag.setVGMDateTime = 0;

	tx_buffer[0]=vgmAddr;
	tx_buffer[1]=0x7f;

	long2bcd( tx_buffer+2,( uint32_t )tiempo.mes,2 );
	long2bcd( tx_buffer+3,( uint32_t )tiempo.dia,2 );
	long2bcd( tx_buffer+4,( uint32_t )tiempo.anio,4 );
	long2bcd( tx_buffer+6,( uint32_t )tiempo.hora,2 );
	long2bcd( tx_buffer+7,( uint32_t )tiempo.min,2 );
	long2bcd( tx_buffer+8,( uint32_t )tiempo.seg,2 );

	i=CRC16( tx_buffer, 9, 0 );
	tx_buffer[9]=i & 0x00ff;
	tx_buffer[10]=( i >> 8 ) & 0x00ff;
	tx_buflen = 11;

	UART0_tx( tx_buffer, tx_buflen );

	// Espera la respuesta
	UART0_rx();

	return rx_buflen;
}

//---------------------------------------------------------------------------------
// Efectúa un long poll y almacena el resultado en el buffer correspondiente.
// Cada vez efectúa un long poll con un código diferente.
// Parámetro: np --> si < 0 realiza el LP que corresponda según la secuencia cíclica
//                   si >= 0 realiza un LP en particular a pedido
// Retorna rx_buflen --> 0 indica que no hubo respuesta.
//---------------------------------------------------------------------------------
uint32_t polling( int np )
{
	int i=0, j, k, n = nPoll;
	uint32_t ret=0;

	if( np >= 0 )
	{
		// Se pidió un LP fuera de la secuencia cíclica
		if( flag.extendedLongPolls )
		{
			// LP extendido --> convierte el nŕo de LP recibido como parámetro
			if( np==NPOLL_COIN_IN )
				n = NPOLL_EXT_COIN_IN;
			else if( np==NPOLL_CURRENT_CREDITS )
				n = NPOLL_EXT_CREDITS;
			else if( np==NPOLL_TOTAL_HANDPAID_CANCELLED_CREDITS )
				n = NPOLL_EXT_HANDPAID_CANCELLED_CREDITS;
			else if( np==NPOLL_FEATURES || np==NPOLL_DENOMINATION )
				n = np;
			else if( np==NPOLL_TOTAL_TICKETS_IN_OUT )
				n = NPOLL_EXT_TOTAL_TICKETS_IN_OUT;
			else
				// No es ninguno de los LP que espera a pedido
			  return 0;
		}
		else
			// Legacy LP
			n = np;
	}

	// Si corresponde setear la fecha y hora de la VGM, en lugar del LP
	// que corresponde hace esto por única vez
	if( flag.setVGMDateTime && flag.fechaHoraValida )
	{
		return setVGMDateTime();
	}

	if( n >= NPOLL_EXTENDED && !flag.extendedLongPolls )
	{
		// La máquina no soporta LP extendidos --> se procesan los LP legacy
		ret = legacyPolling(np);

    return ret;
	}

	tx_buffer[0]=vgmAddr;
	tx_buffer[1]=longPoll[n].lpCommand;

	if( longPoll[n].parameters )
	{
		if( n == NPOLL_FEATURES )
			// El LP A0 lleva como parámetros 00 00, sin longitud
			tx_buflen = 6;
		else
			// El LP 6F lleva parámetros y el 1er byte es la longitud
			tx_buflen = 5 + longPoll[n].parameters[0];

		for(i=0; i<(tx_buflen-2); ++i)
			tx_buffer[i+2] = longPoll[n].parameters[i];

		i=CRC16(tx_buffer,tx_buflen-2,0);

		tx_buffer[tx_buflen-2] = i & 0x00ff;
		tx_buffer[tx_buflen-1] = ( i >> 8 ) & 0x00ff;
	}
	else
	{
		tx_buflen = 2;
	}

	// Envía el LP a la máquina
	UART0_tx( tx_buffer, tx_buflen );

	// Espera la respuesta

	if( !UART0_rx() )   // Obs.: ver si conviene poner la longitud esperada para cada respuesta
	{
	  // La VGM no respondió --> almacena 00 o FF en el campo correspondiente del mensaje
		clearLongPoll(longPoll[n].lpCommand, longPoll[n].parameters);

		// Pasa al siguiente comando para la próxima vuelta
		nPoll = ( nPoll + 1 ) % TOTAL_POLLS;

		return 0;
	}

	/* Verifica que la respuesta recibida sea correcta, y en tal caso la almacena
     en el buffer del mensaje de contadores */

	if( CRC16( rx_buffer,rx_buflen,0 )==0 && rx_buffer[0]==vgmAddr && rx_buffer[1]==longPoll[n].lpCommand )
	{
		if( longPoll[n].parameters == 0 || n == NPOLL_FEATURES )
		{
			// Es un LP normal --> procesa la respuesta según el código del comando del LP
			for(i=0; i<TOTAL_LP_RESPONSES; ++i)
				if( rx_buffer[1] == longPollResponse[i].code )
					for(j=0; j<longPollResponse[i].nBytes; ++j)
						longPollResponse[i].destino[j] = rx_buffer[longPollResponse[i].offset + j];

			if( rx_buffer[1]==0x1f && unidad_denominacion==0 && rx_buffer[7] >= 1 && rx_buffer[7] <= 0x1a )
				// Actualiza la denominación
				unidad_denominacion = denominacion[rx_buffer[7]];

		}
		else if( longPoll[n].lpCommand == 0x6f )
		{
			// Es un LP 6F --> procesa la respuesta según los códigos que hay en su contenido
			k = 5;
			while(k < (rx_buffer[2]+3))
			{
				if( rx_buffer[k+1] == 0 )  // Verifica que el MSB del código sea 00
				{
					if( rx_buffer[k+2] && rx_buffer[k+2]<10 )     // Si la respuesta tiene longitud cero no hace nada, para no
					{                                             // borrar lo que eventualmente se haya escrito en un LP anterior.
						// También descarta si el contador indica más de 9 bytes,
						// y en ese caso deja de procesar el resto de la respuesta porque la
						// longitud es incoherente.
						// Busca en la tabla para determinar dónde almacenar la respuesta
						for(i=0; i<TOTAL_LP6F_RESPONSES; ++i)
						{
							if( rx_buffer[k] == longPollResponse6f[i].code )
							{
								if( isExtendedBillMeter( rx_buffer[k]) )
								{
									// Contadores de billetes van en 4 bytes
									for(j=0; j<4; ++j)
										longPollResponse6f[i].destino[j] = rx_buffer[k + j + 3 + rx_buffer[k+2] - 4];
								}
								else
								{
									// Longitud variable indicada en el primer byte

									// Copia la longitud de la respuesta (1er byte) y luego el valor.
									for(j=0; j<=rx_buffer[k+2]; ++j)
										longPollResponse6f[i].destino[j] = rx_buffer[k + j + 2];
								}
								break;
							}
						}
					}
					else if( rx_buffer[k+2] > 9 )
					{
						break; /* Aborta el while porque no se sabe como procesar el resto del mensaje,
                        debido a que la longitud de un contador es mayor que 9 */
					}
				}
				k += rx_buffer[k+2] + 3;
			}
		}

		if( n == NPOLL_EXT_CREDITS )
		{
			// Actualiza Current Credits
			if( !flag.waitingClientCard && longPollBuffer.lp6f0c[0] <= 9 )
				currentCredits = bcd2long( longPollBuffer.lp6f0c + 1, 2 * longPollBuffer.lp6f0c[0] );
		}
		if( n == NPOLL_EXT_COIN_IN && (features.cashless & DROP_LP1D) )
		{
      /* Agregado para las máquinas IGT viejas, que no mueven el drop del LP1C. Entonces
       se hace un LP1D (exclusivo de IGT) y se le extraen los últimos cuatro bytes, con
       los que se reemplaza el drop del LP1C y el Total Electronics Transfers to VGM del LP2F,
       y los cuatro anteúltimos bytes con los que se reemplaza el Total Electronics
       Transfer to Host del LP2F */
			delay( pollingPeriod );
			UART0_tx_lpr(0x1D);

			if( UART0_rx() && rx_buflen==20 && !CRC16(rx_buffer,rx_buflen,0) && rx_buffer[0]==vgmAddr && rx_buffer[1]==0x1D )
			{
				// Setea las longitudes a 4 bytes
				longPollBuffer.lp6f24[0]=4;  // Total drop
				longPollBuffer.lp6f17[0]=4;  // Total EFT to VGM
				longPollBuffer.lp6f18[0]=4;  // Total EFT to host
				for(i=0; i<4; ++i)
				{
					longPollBuffer.lp6f24[1+i] = rx_buffer[14+i];  // Drop
					longPollBuffer.lp6f17[1+i] = rx_buffer[14+i];  // Total Electronic Transfer to VGM
					longPollBuffer.lp6f18[1+i] = rx_buffer[10+i];  // Total Electronic Transfer to Host
				}
				for(i=5; i<10; ++i)
				{
					// Rellena el resto con ceros
					longPollBuffer.lp6f24[i] = 0;
					longPollBuffer.lp6f17[i] = 0;
					longPollBuffer.lp6f18[i] = 0;
				}
			}
		}
		if( n == NPOLL_FEATURES )
		{
			// Actualiza el período entre pollings y el manejo de LP extendidos
			pollingPeriod = ((longPollBuffer.lpa0[2] & BIT0)? _50ms : _200ms );
			flag.extendedLongPolls = ((longPollBuffer.lpa0[1] & BIT3)? 1 : 0 );
		}
	}
	// Pasa al siguiente comando para la próxima vuelta
	if( np < 0 )
	{
		nPoll = ( nPoll + 1 ) % TOTAL_POLLS;

		if( nPoll == 0 )
		{
			// Completó una vuelta de pollings --> habilita el envío de mensajes de contadores al servidor.
			flag.sendLongPollEnabled = 1;
		}
	}

	return rx_buflen;
}

//------------------------------------------------------------------------------------
// Efectúa un long poll específico a pedido del servidor, y le envía la respuesta.
// Si la VGM no respondió, el campo length va en cero.
// Si esta función es invocada desde el callback de ethernet lleva parámetros:
// dirección IP y puerto de origen, PCB del puerto local, req. de LP a demanda. Esto
// se hace para registrar estos valores para su uso posterior.
// Si la función es invocada desde la tarea SAS, lleva todos los parámetros en cero.
// De esta manera se ejecuta el LP y se envía la respuesta desde el contexto de la
// tarea SAS.
//------------------------------------------------------------------------------------

#define rx ((OnDemandLongPollMsgType *)p.payload)
#define tx ((OnDemandLongPollMsgType *)&rsp)

void OnDemandLongPoll( const ip_addr_t * _addr, uint16_t _port, struct udp_pcb *_upcb, struct pbuf * _p )
{
	int i;
#if 0
	uint8_t b;
#endif
	static OnDemandLongPollMsgType rsp;

	static ip_addr_t addr;         // dirección IP a la que enviar la respuesta
	static uint16_t port;          // Nro de puerto al que enviar la respuesta
	static struct udp_pcb upcb;    // PCB a utilizar para enviar la respuesta
	static struct pbuf p;          // Buffer del mensaje recibido

	if( _addr && _port )
	{
	  // La función fue llamada desde el callback --> registra los datos del requerimiento
	  // para luego responder desde la tarea SAS

	  addr = *_addr;     // dirección IP a la que enviar la respuesta
	  port = _port;      // Nro de puerto al que enviar la respuesta
	  upcb = *_upcb;     // PCB a utilizar para enviar la respuesta
	  p = *_p;           //

    flag.onDemandLongPoll = 1;
	}else if( flag.onDemandLongPoll ) {
	  // La función fue llamada desde la tarea SAS --> ejecuta el LP a demanda y envía la respuesta

	  flag.onDemandLongPoll = 0;

	  if( p.len != sizeof( OnDemandLongPollMsgType ) )
	    return;

	  decifrar( ( uint8_t * )rx, sizeof( OnDemandLongPollMsgType ) );

	  if( CRC16( ( uint8_t * )rx, sizeof( OnDemandLongPollMsgType ),0 ) || !rx->length )
	    return;

	  if( cash_flag || flag.handpayRequest )
	  {
	    // Responde que la VGM está ocupada
	    tx->length = 0xFF;
	  }
	  else
	  {
	    tx_buffer[0] = vgmAddr;

	    for( i=0; i < rx->length; ++i )
	      tx_buffer[i+1] = rx->cmd[i];

	    tx_buflen = i+1;

	    if( rx->length > 1 || rx->cmd[0] < 0x0A )
	    {
	      // Si son comandos de más de un byte o comandos de enable/disable de un byte,
	      // calcula y agrega el CRC. Si no, asume que son comandos de consultas sin CRC,
	      // como por ej.: 01 1A

	      i = CRC16( tx_buffer,tx_buflen,0 );

	      tx_buffer[tx_buflen++] = i & 0x00ff;
	      tx_buffer[tx_buflen++] = ( i >> 8 ) & 0x00ff;
	    }

	    if(rx->length==1)
	    {
	      // Si es un comando de bloquear o desbloquear la VGM, registra el estado en la EEPROM
#if 1
	      if(rx->cmd[0]==0x01)
	      {
	        // bloquear la VGM
	        flag.lockVGM = 1;
	        LCD_print_maquina_bloqueada();
	      }
	      else if(rx->cmd[0]==0x02)
	      {
	        // desbloquear la VGM
	        flag.lockVGM = 0;
	        LCD_print_maquina_libre();
	      }
#else
	      framRead(0x0f30, &b, 2);
	      if(rx->cmd[0]==0x01)
	      {
	        // bloquear la VGM
	        flag.lockVGM = 1;
	        if(b==0)
	        {
	          b=1;
	          framWrite(0x0f30, &b, 2);
	          LCD_print_maquina_bloqueada();
	        }
	      }
	      else if(rx->cmd[0]==0x02)
	      {
	        // desbloquear la VGM
	        flag.lockVGM = 0;
	        if(b==1)
	        {
	          b=0;
	          framWrite(0x0f30, &b, 2);
	          LCD_print_maquina_libre();
	        }
	      }
#endif
	    }

	    delay(pollingPeriod);

	    UART0_tx( tx_buffer, tx_buflen );

	    // Espera la respuesta, verifica que la misma sea correcta, y la almacena
	    if( UART0_rx() && CRC16( rx_buffer,rx_buflen,0 )==0 && rx_buffer[0]==vgmAddr && rx_buffer[1]==rx->cmd[0] )
	    {
	      tx->length = rx_buflen;

	      for( i=0; i<rx_buflen && i<128; ++i )
	        tx->rsp[i] = rx_buffer[i];
	    }
	    else if( tx_buffer[1]==0x21 && rx_buflen==1 && ( rx_buffer[0]&0x7f )==vgmAddr )
	    {
	      // Recibió el ACK o NAK a un LP 21. Si NO es ACK lo reporta al servidor. Si es ACK, luego
	      // enviará la respuesta al LP21 cuando la VGM haya terminado de calcular el ROM signature.
	      // Obs.: la respuesta al LP21 se obtiene posteriormente como respuesta a un GP

	      if( rx_buffer[0]==( vgmAddr|0x80 ) )
	      {
	        // NAK
	        tx->length = rx_buflen;
	        tx->rsp[0] = rx_buffer[0];
	      }
	      else
	      {
	        // ACK: lo descarta porque queda a la espera de la respuesta posterior
	        return;
	      }
	    }
	    else if( tx_buflen>=4 && rx_buflen==1 && ( rx_buffer[0]&0x7f )==vgmAddr )
	    {
	      // Puede ser la respuesta de ACK o NAK a un long poll tipo S
	      if( rx_buffer[0]==( vgmAddr|0x80 ) )
	      {
	        // NAK
	        tx->length = rx_buflen;
	        tx->rsp[0] = rx_buffer[0];
	      }
	      else
	      {
	        // ACK
	        tx->length = rx_buflen;
	        tx->rsp[0] = rx_buffer[0];
	      }
	    }
	    else
	    {
	      // La VGM no respondió
	      tx->length = 0;
	    }
	  }
	  for( i=0; i<3; ++i )
	    tx->mac[i] = MyMAC[i];
	  tx->uid = uid;
	  tx->hash = rand();
	  tx->crc = CRC16( ( uint8_t * )tx, sizeof( OnDemandLongPollMsgType )-2, 0 );
	  cifrar( (uint8_t *) tx, sizeof( OnDemandLongPollMsgType ) );
	  sendUDP( (uint8_t *) tx, sizeof(OnDemandLongPollMsgType), &upcb, &addr, port );
	}
}

#undef rx
#undef tx

#endif

/**
 * @brief  Convierte un valor en uint32_t a una cadena de n dígitos ASCII
 * @param  Puntero a arreglo donde depositar el número como arreglo de ASCII
 * @param  Número a convertir
 * @param  Cantidad de dígitos (para rellenar con ceros a la izquierda)
 * @retval None
 */
void long2str( char * d, uint32_t m, int n )
{
	int i, j;
	sprintf( d, "%u", (unsigned int) m );
	if(n > strlen(d))
	{
		for(i=n, j=strlen(d); i; --i, --j)
			if(j>0)
				d[i-1] = d[j-1];
			else
				d[i-1] = '0';
	}
}

/**
 * @brief  Convierte un valor en uint32_t a una cadena de n dígitos BCD
 * @param  Puntero a arreglo donde depositar el número en BCD
 * @param  Número a convertir
 * @param  Cantidad de dígitos BCD
 * @retval None
 */
void long2bcd( uint8_t * d, uint32_t m, int n )
{
	int i, j;
	char buf[24];
	long2str(buf, m, n);
	for( i=0, j=0; i<n/2; ++i, j+=2 )
	{
		d[i] = ( ( buf[j]-'0' ) << 4 ) | ( ( buf[j+1]-'0' ) & 0x0f );
	}
}

/**
 * @brief  Convierte a un valor en uint32_t una cadena de n dígitos BCD
 * @param  Puntero a arreglo que contiene los dígitos BCD (2 x byte)
 * @param  Cantidad de dígitos BCD
 * @retval Número obtenido en uint32_t
 */
uint32_t bcd2long( uint8_t * d, int n )
{
	int i;
	uint32_t m=0;

	for( i=0; i<n/2; ++i )
	{
		m = m * 100 + ( ( uint32_t )( d[i] >> 4 ) )*10 + ( uint32_t )( d[i] & 0x0f );
	}
	return m;
}

/**
 * @brief Obtiene el estado de créditos de la VGM (LP 0x1A)
 * @param vgm_credits Puntero a la variable donde depositar el resultado
 * @return 0 si no obtuvo respuesta / !=0 si OK --> deja en vgm_credits los créditos
 */
int getCurrentCredits( uint32_t * vgm_credits )
{
	int ret;

	UART0_tx_lpr(0x1A);

	ret = UART0_rx();      // Espera respuesta

	if( ret && rx_buflen == 8 && !CRC16(rx_buffer,rx_buflen,0) )
	{
		*vgm_credits = bcd2long( rx_buffer+2, 8 );
	}
	delay( pollingPeriod );     // Delay entre mensajes de SAS
	UART0_tx_80();             // envía un 0x80 como ACK implícito a la VGM
	delay( pollingPeriod );     // Delay entre mensajes de SAS

	return ret;
}
