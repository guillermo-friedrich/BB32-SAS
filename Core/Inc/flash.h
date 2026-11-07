/*
 * flash.h
 *
 *  Created on: Sep 30, 2026
 *      Author: guillermo
 */

#ifndef INC_FLASH_H_
#define INC_FLASH_H_

// Códigos de mensajes de operaciones sobre la flash

#define FLASH_HIGH_ERASE  1  /* Borrar la parte alta de la flash para iniciar el proceso
                               cambio de versión de firmware */

#define FLASH_BLOCK_STORE 2  /* Almacenar en la flash alta el bloque recibido */

#define FLASH_PROGRAM     3  /* Reprogramar el micro con la imagen almacenada en la flash alta */

// Códigos de error de operaciones de borrado y escritura en flash

#define FLASH_ERROR_ERASE_HIGH   1 // Error al borrar la flash alta
#define FLASH_ERROR_WRITE_HIGH   2 // Error al escribir un bloque en flash alta
#define FLASH_ERROR_VERIFY_HIGH  3 // Error al verificar lo grabado en un bloque de flash alta
#define FLASH_ERROR_CRC_IMAGE    4 // Error al verificar el CRC de la nueva imagen
#define FLASH_ERROR_ERASE_PGM    5 // Error al borrar el sector que contiene a flash_pgm()
#define FLASH_ERROR_WRITE_PGM    6 // Error al escribir la nueva versión de flash_pgm()
#define FLASH_ERROR_INVALID_CODE 9 // Código de operación erróneo

/* Typedefs -----------------------------------------------------------*/

typedef struct __attribute__((packed))
{
  u32_t code;      /* FLASH_HIGH_ERASE: borrar flash alta.
                      FLASH_BLOCK_STORE: datos en buf para grabar en flash alta.
                      FLASH_PROGRAM: reprogramar el micro con la imagen almacenada.
                      en flash alta. */
  u32_t addr;      // Dirección inicial del bloque en buf
  u32_t nbytes;    /* Si code==FLASH_BLOCK_STORE --> cantidad de bytes en buf.
                      Si code==FLASE_PROGRAM --> tamaño total de la nueva imagen de firmware */
  u8_t buf[1024];  /* Si code==FLASH_BLOCK_STORE --> datos a grabar en flash alta
                      Si code==FLASH_PROGRAM --> buf[0] y buf[1] contienen el CRC16 para
                      verificar la integridad de la imagen almacenada en la flash alta */
  u16_t crc;       /* CRC16 para verificar la integridad del mensaje (calculado con
                      el valor inicial MGMT_CRC_SEED */
} FlashPgmMsg_t;

int getConfigFromFlash( FeaturesType * pFeatures, uint32_t * pMyCfgIP );
int saveConfigFlash( FeaturesType *pFeatures, uint32_t ip );
int deleteConfigFlash(void);
void startWatchdog(uint32_t t);

void udp_flash_server_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

void __attribute__((section(".flash_pgm"))) flash_pgm(void);

#endif /* INC_FLASH_H_ */
