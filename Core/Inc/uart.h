/*
 * uart.h
 *
 *  Created on: Oct 4, 2026
 *      Author: guillermo
 */

#ifndef INC_UART_H_
#define INC_UART_H_

// Buffers para la transmisión y recepción con la VGM
extern uint8_t tx_buffer[];
extern int tx_buflen;               // cantidad de bytes almacenados en tx_buffer
extern uint8_t rx_buffer[];
extern volatile int rx_buflen;      // cantidad de bytes almacenados en rx_buffer

void initUART0 ( void );
void UART0_tx ( uint8_t *msg, uint32_t msglen );
void UART0_tx_addr ( uint8_t addr );
void UART0_tx_data ( uint8_t data8 );
void UART0_start_rx (void);
int UART0_rx (void);

#define UART0_tx_81()     {UART0_start_rx();UART0_tx_addr(0x80|vgmAddr);}
#define UART0_tx_80()     {UART0_start_rx();UART0_tx_addr(0x80);}
#define UART0_tx_lpr(cmd) {UART0_start_rx();UART0_tx_addr(vgmAddr);UART0_tx_data(cmd);}

#endif /* INC_UART_H_ */
