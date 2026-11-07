/*
 * ds2401.h
 *
 *  Created on: Sep 26, 2026
 *      Author: guillermo
 */

#ifndef INC_DS2401_H_
#define INC_DS2401_H_

#include "main.h"

/* Resultado de la operación */
typedef enum {
    DS2401_OK    = 0,
    DS2401_ERROR = 1
} DS2401_Status;

/* Leer el número de serie del DS2401
 * mac_addr: buffer de 6 bytes donde se guarda la MAC
 * Retorna DS2401_OK si la lectura fue exitosa */
DS2401_Status DS2401_ReadMAC(uint8_t *mac_addr);

#endif /* INC_DS2401_H_ */
