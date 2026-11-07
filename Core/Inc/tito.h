//-------------------------------------------------------
// tito.h
// Prototipos y declaraciones para tito.c
//-------------------------------------------------------

//#include <stdlib.h>

//void ticket_in(void);               // TITO - Procesamiento de un ticket ingresado
//void ticket_out(void);              // TITO - Procesamiento de un ticket emitido
//unsigned char TITO_cashin(uint32_t monto);  // cargar 'monto' de un ticket a la VGM
//unsigned char TITO_cashin_reject(unsigned char errorCode); // rechazo a una carga con ticket
//unsigned char TITO_cashin_end(void);       // consulta por el �ltimo ticket ingresado (no importa si fue aceptado o rechazado)
//void set_validation_id(void);              // setea validation ID en la VGM
//void validation_system_ticket_out(void);   // TITO - Procesamiento de un ticket emitido con validaci�n system
//void set_ticket_data(void);                // Setea los datos fijos a imprimir en los tickets

/*
 * tito.h
 *
 *  Created on: 18 ene. 2020
 *      Author: guillermo
 */

#ifndef INC_TITO_H_
#define INC_TITO_H_

//--- Formatos de LP usados en TITO

// TITO - Send Enhanced Validation Info (Respuesta a un LP 4D)

typedef struct __attribute__((packed))
{
  uint8_t  vgmAddr;
  uint8_t  command;         // 0x4D
  uint8_t  validation_type;
  uint8_t  index_number;
  uint8_t  date[4];
  uint8_t  time[3];
  uint8_t  validation_number[8];
  uint8_t  amount[5];
  uint16_t ticket_number;
  uint8_t  validation_system_id;
  uint8_t  expiration[4];
  uint16_t poll_id;
  uint16_t crc;
} EnhancedValidationInfoType;

// TITO - Cashout Ticket Information (Respuesta a un LP 3D)

typedef struct __attribute__((packed))
{
  uint8_t  vgmAddr;
  uint8_t  command;         // 0x3D
  uint8_t  validation_number[4];
  uint8_t  amount[5];
  uint16_t crc;
} CashoutTicketInfoType;

// TITO - Send Ticket Validation Data (Respuesta a un LP 70)
typedef struct __attribute__((packed))
{
  uint8_t  vgmAddr;
  uint8_t  command;         // 0x70
  uint8_t  length;
  uint8_t  ticket_status;
  uint8_t  ticket_amount[5];
  uint8_t  parsing_code;
  uint8_t  validation_data[9];
  uint16_t crc;
} TicketValidationDataType;

// TITO - Redeem Ticket Message (LP 71)
typedef union __attribute__((packed))
{
  struct __attribute__((packed))
  {
    uint8_t  vgmAddr;
    uint8_t  command;         // 0x71
    uint8_t  length;          // 16
    uint8_t  transfer_code;   // 0x00: valid cashable ticket - 0x80: unable to validate
    uint8_t  transfer_amount[5];   // monto en centavos, en BCD
    uint8_t  parsing_code;         // 0x00 --> validation number de 18 dígitos BCD
    uint8_t  validation_data[9];   // validation number de 18 dígitos BCD
    uint16_t crc;
  } req;

  struct __attribute__((packed))
  {
    uint8_t  vgmAddr;
    uint8_t  command;         // 0x71
    uint8_t  length;          // 1
    uint8_t  transfer_code;   // 0xff: request for current ticket status
    uint16_t crc;
  } cons;

  struct __attribute__((packed))
  {
    uint8_t  vgmAddr;
    uint8_t  command;         // 0x71
    uint8_t  length;
    uint8_t  machine_status;
    uint8_t  amount[5];
    uint8_t  parsing_code;    // 0x00
    uint8_t  validation_data[9];
    uint16_t crc;
  } resp;
} RedeemTicketType;

typedef struct __attribute__((packed))
{
  uint8_t  vgmAddr;
  uint8_t  command;         // 0x4C
  uint8_t  machine_id[3];
  uint8_t  sequence_number[3];
  uint16_t crc;
} SetSecureEnhancedValidationIDType;

// Respuesta a un LP 57 (luego de un evento 57, que informa que hay un ticket
// a ser impreso con validación system).

typedef struct __attribute__((packed))
{
  uint8_t vgmAddr;
  uint8_t command;         // 0x57
  uint8_t cashout_type;    // 0x00: cashable ticket - 0x01: restricted promotional ticket -
                           // 0x80: not waiting for system validation
  uint8_t  amount[5];      // Monto del ticket en centavos
  uint16_t crc;
} PendingCashoutInfoType;

// Comando a enviar a la VGM luego de recibir la respuesta al LP 57, para darle el
// validation number a imprimir en el ticket

typedef struct __attribute__((packed))
{
  uint8_t  vgmAddr;
  uint8_t  command;             // 0x58
  uint8_t  validationSystemID;  // BCD - Usar 0x01 - 0x00 se usa para denegar la validación system
  uint8_t  validationNumber[8]; // validation number a imprimir en el ticket
  uint16_t crc;
} ReceiveValidationNumberCommandType;

// Respuesta al LP 58

typedef struct __attribute__((packed))
{
  uint8_t  vgmAddr;
  uint8_t  command;    // 0x58
  uint8_t  status;     // 0x00: command acknowledged - 0x80: not in cashout - 0x81: improper validation rejected
  uint16_t crc;
} ReceiveValidationNumberReponseType;

// Códigos de machine status para las respuestas al LP 71
// Los códigos del 0x80 al 0x81 son códigos de error, que se reportan al servidor
// Obs.: el campo <protocol> en 'T' permite discriminar que <errorCode> proviene de TITO

#define TITO_CASHABLE_TICKET_REDEEMED   0x00   // Idem AFT
#define TITO_TICKET_REDEMPTION_PENDING  0x40   // Idem AFT

// Los siguientes están asimilados a los códigos de error de AFT
#define TITO_NO_RESPONSE         0xE1  // AFT_NO_RESPONSE
#define TITO_BAD_RESPONSE        0xE2  // AFT_BAD_RESPONSE
#define TITO_SERVER_NO_RESPONSE  0xE3  // AFT_SERVER_NO_RESPONSE

// Códigos de Ticket Transfer (se usan en el LP 71, para ordenar la carga de la VGM
// o el rechazo -y expulsión- del ticket).
// Obs.: los códigos entre 0x81 y 0x88 explicitan ciertos errores, pero no los usamos.

#define TITO_VALID_CASHABLE_TICKET     0x00   // OK: cargar monto
#define TITO_UNABLE_TO_VALIDATE        0x80   // Rechazar la carga
#define TITO_REQUEST_TICKET_STATUS     0xff   // Consultar estado del último ticket

// Códigos de cashout type (viene en la respuesta al LP 57)
#define TITO_CASHABLE_TICKET                   0x00
#define TITO_RESTRICTED_PROMOTIONAL_TICKET     0x01
#define TITO_NOT_WAITING_FOR_SYSTEM_VALIDATION 0x80

// Prototipos de funciones de tito.c
void ticket_in(void);                          // TITO - Procesamiento de un ticket ingresado
void ticket_out(void);                         // TITO - Procesamiento de un ticket emitido
uint8_t TITO_cashin(uint32_t monto);           // cargar 'monto' de un ticket a la VGM
uint8_t TITO_cashin_reject(uint8_t errorCode); // rechazo a una carga con ticket
uint8_t TITO_cashin_end(void);                 // consulta por el último ticket ingresado (no importa si fue aceptado o rechazado)
void set_validation_id(void);                  // setea validation ID en la VGM
void validation_system_ticket_out(void);       // TITO - Procesamiento de un ticket emitido con validación system
void set_ticket_data(void);                    // Setea los datos fijos a imprimir en los tickets

#endif /* INC_TITO_H_ */
