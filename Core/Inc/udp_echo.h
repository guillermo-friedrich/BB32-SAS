/*
 * udp_echo.h
 *
 *  Created on: Sep 27, 2026
 *      Author: guillermo
 */

#ifndef UDP_ECHO_H
#define UDP_ECHO_H

#include "lwip/udp.h"

/* Puerto UDP de escucha */
#define UDP_ECHO_PORT    7   /* puerto estándar de echo, o cambiarlo al que necesites */

/* Inicializar el servidor UDP echo */
void UDP_Echo_Init(void);

/* Procesar — llamar periódicamente desde el loop principal */
void UDP_Echo_Process(void);

#endif /* UDP_ECHO_H */
