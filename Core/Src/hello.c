/*
 * hello.c
 *
 *  Created on: 27 nov. 2019
 *      Author: guillermo
 */

/* Includes ------------------------------------------------------------------*/
#include "cashless.h"   // Éste incluye a todos los demás *.h

#define HW_BB32 3

/* Private variables */

static volatile uint32_t sendHelloPeriod = _15seg;  /* Período para el envío de Hello */

// Flag que indica si el servidor está vivo. Se activa
// al recibir la respuesta al Hello -con fecha y hora-
static uint32_t serverAlive;

FechaHoraType tiempo;

/*--------------------------------------------------------------
*  Envío periódico del Hello y procesamiento de las respuestas
*-------------------------------------------------------------*/
void Hello_process()
{
  static uint32_t thello, tWaitingReply;
  uint32_t t0;

	if( (HAL_GetTick()-thello) > (sendHelloPeriod-tWaitingReply) )
	{
	  thello = HAL_GetTick();

    if( flag.watchdogInit ) {
      sendHello( "WDT INIT", "" );
      flag.watchdogInit = 0;
    }else
      sendHello( VERSION, "" );

    if( serverAlive )
      --serverAlive;
    else if( !flag.cardStatus )
    {
      // El servidor no responde, pone un mensaje y reduce el período entre mensajes Hello
      LCD_print( "FUERA DE", "SERVICIO" );
      sendHelloPeriod = _5seg;
    }

    flag.waitingHelloReply = 1;

    t0 = HAL_GetTick();
    while( flag.waitingHelloReply && (HAL_GetTick()-t0)<_3seg )
    {
      MX_LWIP_Process();
      delay(pollingPeriod);
      UART0_tx_80();
    }
    tWaitingReply = HAL_GetTick() - t0;
  }
}

//---------------------------------------------------------
// Verifica el mensaje de Eco Reply recibido del servidor,
// y actualiza fecha y hora
//---------------------------------------------------------

void processHelloReply( struct pbuf *udp_rx_buf )
{
  int i;
  HelloReplyMsgType *p = udp_rx_buf->payload;

  if( udp_rx_buf->len == sizeof( HelloReplyMsgType ) )
  {
    decifrar( (uint8_t *)p, sizeof( HelloReplyMsgType ) );

    if( !CRC16( (uint8_t *)p, sizeof( HelloReplyMsgType ), 0 ) && p->msgID[0]=='F' && p->msgID[1]=='H' )
    {
      flag.waitingHelloReply = 0;

      if( !serverAlive && !flag.cardStatus && !getCardStatus() )
        // Se restableció la conexión con el servidor
        LCD_print_maquina_libre();

      // Por las dudas, cada vez que envía un Hello reduce el período a 5 seg
      // Cada vez que recibe una respuesta lo restaura a 15 seg. De esa
      // manera, si se pierde la conexión con el servidor se detecta más rápido
      // y si la conexión se mantiene, no se altera nada.
      sendHelloPeriod = _15seg;

      serverAlive = 3;            // (el 3 es para permitir tres respuestas perdidas)

      // Obtiene la fecha y hora enviada por el servidor
      tiempo.anio=tiempo.mes=tiempo.dia=tiempo.hora=tiempo.min=tiempo.seg=0;

      for( i=0; i<4; ++i )
        tiempo.anio = tiempo.anio * 10 + ( p->anio[i] - '0' );

      for( i=0; i<2; ++i )
      {
        tiempo.mes  = tiempo.mes * 10  + ( p->mes[i]  - '0' );
        tiempo.dia  = tiempo.dia * 10  + ( p->dia[i]  - '0' );
        tiempo.hora = tiempo.hora * 10 + ( p->hora[i] - '0' );
        tiempo.min  = tiempo.min * 10  + ( p->min[i]  - '0' );
        tiempo.seg  = tiempo.seg * 10  + ( p->seg[i]  - '0' );
      }
      flag.fechaHoraValida = 1;

      // Actualiza la cantidad de reintentos para el envío de mensajes de eventos
      // al servidor antes de bloquear la VGM, en caso de no tener respuesta del
      // servidor.

      nRetry2LockVGM = p->nRetry2LockVGM;

      uid = p->uid;

      // Actualiza la modalidad de trabajo de TITO y/o cashless,
      // en función de lo ordenado desde el servidor.
/*
      setTitoCashlessOptions(p->opcion_tito, p->opcion_cashless);

      // Si es una máquina EIBE o una AFT que admite carga de créditos para
      // bola extra en medio de una jugada, setea el flag correspondiente
      flag.enableBolaExtra = ( vgmProto == EIBE || ( Features.cashless==CASHLESS_ENABLED_BOLA_EXTRA ) )?1:0;
*/
      switch( p->comando )
      {
      case 0x01:
        // IP duplicada. Si la MAC coincide con la propia se debe reiniciar el módulo
        if( p->mac[0] == MyMAC[0] )
          if( p->mac[1] == MyMAC[1] )
            if( p->mac[2] == MyMAC[2] )
            {
              // El servidor detectó IP duplicada y ordenó reiniciar el módulo
              // Se queda en un loop esperando a que el WDT haga resetear al micro.
              startWatchdog( _10seg ); // Esto es para no tener que esperar los 10 seg del WDT
              while( 1 );
            }
        break;
      case 0x02:
        // IP duplicada. Si la MAC coincide con la propia se debe bloquear el módulo
        {
          char buf[16];
          sprintf( buf, "%lu.%lu.%lu.%lu", MyIP()&0xff, (MyIP()>>8)&0xff, (MyIP()>>16)&0xff, (MyIP()>>24)&0xff );
          LCD_print( _IP_DUPLICADA, buf );

          // Envía un Hello al servidor, a fin de que registre la IP repetida
          sendHello( "DUP ", buf );
        }
        flag.lockModule = 1;  // Ordena bloquear el módulo
        break;

      case 0x03:
        // Configurar el UID del módulo con el indicado en "uid"
        // Previamente verifica si es necesario grabar.
        uid = p->uid;
        if( features.uid != uid )
          setUID( uid );
        break;

      case 0x04:
        // Si el UID configurado no es el que corresponde a la VGM se debe bloquear el módulo
        {
          char buf[16];
          sprintf( buf, "%u - (%u)", (unsigned int)uid, (unsigned int)p->uid );
          LCD_print( "UID ERRONEO", buf );

          // Envía un Hello al servidor, a fin de que registre la IP repetida
          sendHello( "UID ", buf );
        }
        flag.lockModule = 1;  // Ordena bloquear el módulo

      case 0x05:
        // Cambiar la IP del Mistery. Previamente verifica si debe cambiarla
        if( IPV4(p->IpMistery[0], p->IpMistery[1], p->IpMistery[2], p->IpMistery[3]) != features.IP_server_prog )
          setIpMistery( p->IpMistery );
        break;

      }
/*
      // Setear el monto máximo para emitir un ticket con validación System (centavos)
      // o el monto máximo para resetear en forma remota un pago manual (créditos)
      if( p->maxCashout[0] )
        montoMaxTicket = strtoul( p->maxCashout,0,10 );
      else
        montoMaxTicket = 0;
*/
      // Verifica si debe bloquear o desbloquear la VGM
      if( p->vgmLockStatus && !flag.lockVGM )
      {
        // La VGM está operativa y se la debe bloquear
        DisableVGM();
        flag.lockVGM = i = 1;
//        framWrite(0x0f30,(unsigned char *)&i, 2);
      }
      else if(!p->vgmLockStatus && flag.lockVGM)
      {
        // La VGM está bloqueada y se la debe desbloquear
        EnableVGM();
        flag.lockVGM = i = 0;
//        framWrite(0x0f30,(unsigned char *)&i, 2);
      }
    }
  }
}

//--------------------------------------------------------------
// Envía un mensaje de Hello con los dos textos pasados como
// parámetro concatenados en "version"
//--------------------------------------------------------------
void sendHello (char *s1, char *s2)
{
  int i;
  static u16_t n = 0;
  uint32_t v = Vcc();
  static HelloMsgType helloMsg;

  strcpy (helloMsg.hello, "HELLO");

  strncpy (helloMsg.version, s1, 20);
  if( strlen(s1) < 20 )
    strncat (helloMsg.version, s2, 20-strlen(s1));

  helloMsg.version[19] = 0;  // Por si acaso la longitud de los carteles excede es mayor
                             // de 19 char, pone un caracter nulo en la última posición

  sprintf (helloMsg.vcc, "%lu.%lu", v / 1000, v % 1000);
  helloMsg.vcc[5] = 0;

  helloMsg.vgmProto = cfg.eibe | ( HW_BB32<<4 ); // 4 LSB: Protocolo (0:SAS 1:EIBE)
                                                 // 4 MSB: Tipo de placa HW_OLIMEX (1) o HW_BB3 (2) o HW_BB32 (3)
  helloMsg.uid = uid;

  helloMsg.vgmLockStatus = flag.lockVGM;

  for (i = 0; i < 6; ++i)
    helloMsg.mac[i] = MyMAC[i];

  helloMsg.IpMistery = addr_serv_prog.addr;

  helloMsg.opcion_cashless = (cfg.cashlessBolaExtra ? 3 : 1);

  helloMsg.hash = (rand () & 0xff00) | (n++ & 0x00ff);

  helloMsg.crc = CRC16 ((unsigned char*) &helloMsg, sizeof(HelloMsgType) - 2, 0);

  cifrar ((unsigned char*) &helloMsg, sizeof(HelloMsgType));

  flag.waitingHelloReply = 1;

  sendUDP ( (uint8_t *) &helloMsg, sizeof(HelloMsgType), upcb_cli, &addr_serv_online, SERV_PORT_HELLO );
}

