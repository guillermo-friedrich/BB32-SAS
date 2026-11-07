/*
 * eventos.h
 *
 *  Created on: 3 dic. 2019
 *      Author: guillermo
 */

#ifndef INC_EVENTOS_H_
#define INC_EVENTOS_H_

#define MAX_EVENTOS 32     /* cantidad máxima de eventos por mensaje */

/* Código numérico de cada evento */

#define EVT_11_MAIN_DOOR_OPEN              0x11
#define EVT_12_MAIN_DOOR_CLOSED            0x12
#define EVT_15_CARD_CAGE_OPEN              0x15
#define EVT_16_CARD_CAGE_CLOSED            0x16
#define EVT_19_CASH_DOOR_OPEN              0x19
#define EVT_1A_CASH_DOOR_CLOSED            0x1A
#define EVT_1B_CASHBOX_WAS_REMOVED         0x1B
#define EVT_1C_CASHBOX_WAS_INSTALLED       0x1C
#define EVT_1F_WAITING_USER_INPUT          0x1F
#define EVT_21_COIN_IN_TILT                0x21
#define EVT_27_CASHBOX_FULL_DETECTED       0X27
#define EVT_28_BILL_JAM                    0X28
#define EVT_29_BILL_ACCEPTOR_HW_FAILURE    0x29
#define EVT_2B_BILL_REJECTED               0x2B
#define EVT_2C_COUNTERFEIT_BILL_REJECTED   0x2C
#define EVT_33_CMOS_RAM_BAD_DEVICE         0x33
#define EVT_3A_MEMORY_ERROR_RESET          0x3A
#define EVT_3B_POWER_SAVE_ERROR            0x3B
#define EVT_3D_CASHOUT_TICKET_PRINTED      0x3D
#define EVT_3E_HANDPAY_HAS_BEEN_VALIDATED  0x3E
#define EVT_3F_VALIDATION_ID_NOT_CONFIGURED 0x3F
#define EVT_47_1_BILL_ACCEPTED             0x47
#define EVT_48_5_BILL_ACCEPTED             0x48
#define EVT_49_10_BILL_ACCEPTED            0x49
#define EVT_4A_20_BILL_ACCEPTED            0x4A
#define EVT_4B_50_BILL_ACCEPTED            0x4B
#define EVT_4C_100_BILL_ACCEPTED           0x4C
#define EVT_4D_2_BILL_ACCEPTED             0x4D
#define EVT_4E_500_BILL_ACCEPTED           0x4E
#define EVT_4F_BILL_ACCEPTED               0x4F
#define EVT_50_200_BILL_ACCEPTED           0x50
#define EVT_51_HANDPAY_PENDING             0x51
#define EVT_52_HANDPAY_RESET               0x52
#define EVT_54_PROGRESSIVE_WIN             0x54
#define EVT_57_SYSTEM_VALIDATION_REQUEST   0x57
#define EVT_60_PRINTER_COMMUNICATION_ERROR 0x60
#define EVT_61_PRINTER_PAPER_OUT_ERROR     0x61
#define EVT_66_CASHOUT_BUTTON_PRESSED      0x66
#define EVT_67_VOUCHER_INSERTED            0x67
#define EVT_68_VOUCHER_TRANSFER_COMPLETE   0x68
#define EVT_69_AFT_TRANSFER_COMPLETE       0x69
#define EVT_6A_AFT_REQ_4_HOST_CASHOUT      0x6A
#define EVT_6B_AFT_REQ_4_HOST_CASHOUT_WIN  0x6B
#define EVT_6F_GAME_LOCKED                 0x6F
#define EVT_74_PRINTER_PAPER_LOW           0x74
#define EVT_75_PRINTER_POWER_OFF           0x75
#define EVT_76_PRINTER_POWER_ON            0x76
#define EVT_77_REPLACE_PRINTER_RIBBON      0x77
#define EVT_78_PRINTER_CARRIAGE_JAMMED     0x78
#define EVT_7C_LEGACY_BONUS_PAYED          0x7C
#define EVT_7E_GAME_STARTED                0x7E
#define EVT_7F_GAME_ENDED                  0x7F
#define EVT_81_HOPPER_LEVEL_LOW            0x81
#define EVT_82_ATTENDANT_MENU_ENTER        0x82
#define EVT_83_ATTENDANT_MENU_EXIT         0x83
#define EVT_84_SELF_TEST_MENU_ENTER        0x84
#define EVT_85_SELF_TEST_MENU_EXIT         0x85
#define EVT_8C_GAME_SELECTED               0x8C

/* Eventos generados por el módulo (no existen en SAS */
#define EVT_F0_VGM_DISCONNECTED           0xF0
#define EVT_F1_VGM_CONNECTED              0xF1

/*
 * Estructura del mensaje de eventos enviado al servidor7
 */
typedef struct __attribute__((packed))
{
  char     msgID[2];            // Id. del mensaje de Eventos --> "EV"
  uint8_t  nSec;                // Nro de secuencia del mensaje
  uint8_t  nEventos;            // Cantidad de eventos contenidos en el mensaje
  uint8_t  evento[MAX_EVENTOS]; // arreglo para almacenar los eventos obtenidos de la máquina
  char     fechaHora[14];       // fecha y hora en ASCII: AAAAMMDDhhmmss
  uint8_t  mac[6];              // dirección MAC del módulo
  uint16_t uid;                 // UID de la VGM
  uint16_t hash;
  uint16_t crc;
} EventMsgType;

/*
 * Estructura del mensaje de ACK de eventos enviado como respuesta por el servidor
 */
typedef struct __attribute__((packed))
{
  char     msgID[2];  // Id. del mensaje de Ack de eventos --> "EV"
  uint8_t  nSec;      // Nro de secuencia del mensaje
  uint8_t  codError;  // Código de error (0=OK, 1=nSec repetido)
  uint16_t hash;
  uint16_t crc;
} EventAckType;

extern EventMsgType bufEventos;       // Buffer del mensaje de eventos

#endif /* INC_EVENTOS_H_ */
