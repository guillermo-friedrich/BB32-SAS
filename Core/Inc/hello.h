/*
 * hello.h
 *
 *  Created on: Oct 9, 2026
 *      Author: guillermo
 */

#ifndef INC_HELLO_H_
#define INC_HELLO_H_

/*-----------------------------------------------
 * Estructura para almacenar la fecha y hora
 *-----------------------------------------------
 */
typedef struct
{
  int anio;
  int mes;
  int dia;
  int hora;
  int min;
  int seg;
  int ticks;     // contador de "ticks". Se incrementa cada 10 ms por la ISR
} FechaHoraType;

// Variable que almacena aaaa/mm/dd hh:mm:ss y se actualiza con la fecha y hora del servidor
extern FechaHoraType tiempo;

/*---------------------------------------------------------------------------------
 *  Estructura del mensaje de Hello (Echo Request), que el módulo usa para detectar
 *  la presencia del servidor e informarle la versión de firmware
 *---------------------------------------------------------------------------------
 */
typedef struct __attribute__((packed))
{
  char hello[6];         // Id. del mensaje --> "HELLO"
  char version[20];      // Hasta 19 ASCII + null
  char vcc[6];           // 5 dígitos + null - Ej.: 3.258
  uint8_t vgmProto;      // SAS - DACOM - GAMMA - EIBE
  uint8_t vgmLockStatus; // 0: VGM operativa - 1-FF: VGM bloqueada
  uint16_t uid;          // UID de la VGM
  uint8_t mac[6];        // dirección MAC del módulo
  uint32_t IpMistery;    // dirección IP del controlador del Mistery (0.0.0.0 --> no hay)
  uint8_t opcion_tito;   // Informa configuración de TITO:
                         // 01: TITO habilitado
                         // Otro valor: TITO deshabilitado

  uint8_t opcion_cashless;   // Informa configuiración de cashless:
                             // 00: cashless deshabilitado (sólo online)
                             // 01: cashless habilitado
                             // 02: cashless deshabilitado (sólo online) - Ruletas Golden
                             // 03: cashless habilitado - permite carga de créditos en medio del juego, para bola extra
  uint16_t hash;
  uint16_t crc;
} HelloMsgType;

/* ------------------------------------------------------------------------------
 * Estructura del mensaje de Echo Reply, que transporta fecha y hora, y el monto
 *  de carga unitaria configurado para los vendedores
 *  ------------------------------------------------------------------------------
 */
typedef struct __attribute__((packed))
{
  char msgID[2];   // Id. del mensaje de Fecha y Hora --> "FH"
  char anio[4];    // Año: 4 dígitos ASCII
  char mes[2];     // Mes: 2 dígitos ASCII ("01" al "12")
  char dia[2];     // Día: 2 dígitos ASCII ("01" al "31")
  char hora[2];    // Hora: 2 dígitos ASCII ("00" al "23")
  char min[2];     // Minutos: 2 dígitos ASCII ("00" al "59")
  char seg[2];     // Segundos: 2 dígitos ASCII ("00" al "59")

  /* Cantidad de intentos en el envío de eventos al servidor,
   * y las acciones correspondientes:
   * 0 : si después de 6 envíos no recibió el ACK del
   * servidor, guarda el mensaje en EEPROM para
   * evitar la pérdida de los eventos en caso de
   * reinicio del módulo y sigue intentando
   * indefinidamente, pero luego del 12° intento
   * bloquea la VGM.
   * 1-255 : Cantidad de envíos de un mensaje de eventos,
   * antes de bloquear la máquina
   */
  uint8_t nRetry2LockVGM;

  /* Comando a ejecutar en función de la MAC o el UID
   * 0 : nada
   * 1 : reiniciar módulo cuya MAC coincide con la indicada en "mac"
   * 2 : bloquear módulo cuya MAC coincide con la indicada en "mac"
   * 3 : setear el valor de UID desde "uid"
   * 4 : bloquear módulo por UID incorrecto
   * 5 : setear la IP del controlador de progresivo desde "ipMistery"
   */
  uint8_t comando;

  uint8_t mac[6];     // MAC del módulo (se usa en función de "comando")

  /* 0 --> no se tiene en cuenta
   * otro valor: monto máximo para ticket en System (centavos)
   * monto máximo para reset remoto de pago manual (créditos)
   */
  char maxCashout[17];

  uint8_t vgmLockStatus; // 0: VGM operativa / desbloquear -- 1-FF: Bloquear VGM

  uint16_t uid;     // UID de la VGM (se usa en función de "comando")

  uint8_t IpMistery[4];    // IP del controlador de Mistery (0.0.0.0 --> no hay)

  uint8_t opcion_tito;     // 01: TITO habilitado
                           // Otro valor: TITO deshabilitado

  /* 00: cashless deshabilitado (sólo online)
   * 01: cashless habilitado
   * 02: cashless deshabilitado (sólo online) - Ruletas Golden
   * 03: cashless habilitado - permite carga de créditos en medio del juego, para bola extra
   */
  uint8_t opcion_cashless;

  uint8_t IpServCashTito[4];     // IP del servidor de cashless o TITO

  uint8_t reservado[4];          // 4 bytes reservados para uso futuro

  uint16_t hash;
  uint16_t crc;
} HelloReplyMsgType;

void Hello_process(void);
void processHelloReply( struct pbuf *udp_rx_buf );
void sendHello (char *s1, char *s2);

#endif /* INC_HELLO_H_ */
