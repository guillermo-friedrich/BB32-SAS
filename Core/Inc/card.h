/*
 * card.h
 *
 *  Created on: Sep 28, 2026
 *      Author: guillermo
 */

#ifndef INC_CARD_H_
#define INC_CARD_H_

#define DO_CASHIN 1
#define DO_CASHOUT 2

struct flags
{
  uint32_t cardStatus : 1;
};

//extern struct flags flag;

extern uint8_t cardID[];   // ID de cliente o vendedor grabado en la tarjeta

//extern int cash_flag;

int getCardStatus(void);
void checkCardReader(void);

#endif /* INC_CARD_H_ */
