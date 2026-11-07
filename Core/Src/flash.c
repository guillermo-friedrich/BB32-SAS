/*
 * flash.c
 *
 *  Created on: Sep 30, 2026
 *      Author: guillermo
 */

#include "cashless.h"
#include "stm32f4xx_hal_flash.h"

/* Private define ------------------------------------------------------------*/
#define SECTOR_MASK               ((uint32_t)0xFFFFFF07)

/* Private macro -------------------------------------------------------------*/
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base @ of Sector 0, 16 Kbytes */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base @ of Sector 1, 16 Kbytes */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base @ of Sector 2, 16 Kbytes */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base @ of Sector 3, 16 Kbytes */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base @ of Sector 4, 64 Kbytes */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base @ of Sector 5, 128 Kbytes */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base @ of Sector 6, 128 Kbytes */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base @ of Sector 7, 128 Kbytes */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base @ of Sector 8, 128 Kbytes */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base @ of Sector 9, 128 Kbytes */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base @ of Sector 10, 128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base @ of Sector 11, 128 Kbytes */

#define ADDR_FLASH_SECTOR_12    ((uint32_t)0x08100000) /* Base @ of Sector 12, 16 Kbytes */
#define ADDR_FLASH_SECTOR_13    ((uint32_t)0x08104000) /* Base @ of Sector 13, 16 Kbytes */
#define ADDR_FLASH_SECTOR_14    ((uint32_t)0x08108000) /* Base @ of Sector 14, 16 Kbytes */
#define ADDR_FLASH_SECTOR_15    ((uint32_t)0x0810C000) /* Base @ of Sector 15, 16 Kbytes */
#define ADDR_FLASH_SECTOR_16    ((uint32_t)0x08110000) /* Base @ of Sector 16, 64 Kbytes */
#define ADDR_FLASH_SECTOR_17    ((uint32_t)0x08120000) /* Base @ of Sector 16, 128 Kbytes */
#define ADDR_FLASH_SECTOR_18    ((uint32_t)0x08140000) /* Base @ of Sector 16, 128 Kbytes */
#define ADDR_FLASH_SECTOR_19    ((uint32_t)0x08160000) /* Base @ of Sector 16, 128 Kbytes */
#define ADDR_FLASH_SECTOR_20    ((uint32_t)0x08180000) /* Base @ of Sector 16, 128 Kbytes */
#define ADDR_FLASH_SECTOR_21    ((uint32_t)0x081A0000) /* Base @ of Sector 16, 128 Kbytes */
#define ADDR_FLASH_SECTOR_22    ((uint32_t)0x081C0000) /* Base @ of Sector 16, 128 Kbytes */
#define ADDR_FLASH_SECTOR_23    ((uint32_t)0x081E0000) /* Base @ of Sector 16, 128 Kbytes */

/**
 * @brief Esta función es ejecutada cuando se recibe un datagrama UDP destinado
 *        al proceso de reprogramación de la memoria flash.
 * @param arg argumentos provistos por el usuario (udp_pcb.recv_arg)
 * @param pcb bloque de control udp_pcb del paquete recibido
 * @param p puntero al buffer que contiene el paquete recibido
 * @param addr dirección IP remota desde la que proviene el datagrama recibido
 * @param port puerto remoto desde donde proviene el datagrama recibido
 * @return None
 */
void udp_flash_server_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  struct pbuf *rsp;
  FlashPgmMsg_t *msg = p->payload;
  int32_t i, error = 0, flashear = 0;
  volatile u32_t Address = 0, *data32;
  FLASH_EraseInitTypeDef EraseInitStruct;
  uint32_t SectorError = 0;

  if((addr->addr == addr_serv_online.addr || addr->addr == addr_serv_mgmt.addr) && (port == 1237) && (p->len == sizeof(FlashPgmMsg_t)) && !CRC16((uint8_t *) msg, p->len, MGMT_CRC_SEED))
  {
    switch(msg->code)
    {
    case FLASH_HIGH_ERASE:
      flag.updatingFirmware = 1;

      /* Borra los sectores 12 a 23 de la flash, usados para almacenar temporariamente
         la nueva imagen del firmware. */

      /* Desbloquea la Flash para habilitar el acceso al registro de control
         Nota: HAL limpia los flags internamente al desbloquear y antes de operar */
      HAL_FLASH_Unlock();

      /* Configurar el borrado masivo de los 12 sectores consecutivos (del 12 al 23) */
      EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
      EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3; // Tensión entre 2.7V y 3.6V
      EraseInitStruct.Sector        = FLASH_SECTOR_12;       // Sector inicial
      EraseInitStruct.NbSectors     = 12;                     // Cantidad de sectores a borrar (12 al 23)

      /* Ejecuta el borrado de los sectores en una sola llamada */
      if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
      {
        /* Si ocurre un error, 'SectorError' contendrá el número del sector que falló.
           El usuario puede agregar aquí código para manejar este error */
        error = FLASH_ERROR_ERASE_HIGH;
      }

      /* Bloquea la Flash para deshabilitar el acceso al registro de control de la flash,
         a fin de proteger el contenido de esta memoria de cualquier operación no deseada */
      HAL_FLASH_Lock();

      break;

    case FLASH_BLOCK_STORE:
      // Graba el fragmento recibido

      /* Desbloquea la Flash para habilitar el acceso al registro de control
         Nota: HAL limpia internamente los flags al desbloquear y antes de operar */
      HAL_FLASH_Unlock();

      /* Inicializa las variables para el bucle de grabación */
      Address = ADDR_FLASH_SECTOR_12 + msg->addr;
      data32 = (uint32_t *)(msg->buf);

      /* Deshabilita interrupciones globales (reemplaza a FreeRTOS) */
      __disable_irq();

      for (i = 0; i < msg->nbytes; i += 4)
      {
        /* Graba una palabra de 32 bits (Word) */
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, *data32) == HAL_OK)
        {
          Address += 4;
          data32++;
        }
        else
        {
          error = FLASH_ERROR_WRITE_HIGH;
          break;
        }
      }

      /* Habilita interrupciones globales (reemplaza a FreeRTOS) */
      __enable_irq();

      /* Bloquea la Flash para proteger el contenido de operaciones no deseadas */
      HAL_FLASH_Lock();

      if (error)
        break; // Si ya hubo error, sale sin verificar

      /* Verifica lo grabado (Lectura directa de memoria) */
      Address = ADDR_FLASH_SECTOR_12 + msg->addr;
      data32 = (uint32_t *)msg->buf;

      for (i = 0; i < msg->nbytes; i += 4, data32++)
      {
        /* Nota: Para leer la flash no necesitas desbloquearla, se lee directamente
           apuntando a la dirección de memoria física con un puntero */
        if (*(__IO uint32_t *)(Address + i) != *data32)
        {
          error = FLASH_ERROR_VERIFY_HIGH;
          break;
        }
      }
      break;

    case FLASH_PROGRAM:
      // Reprograma el micro a partir de la imagen almacenada en la parte alta de la flash.

      // Primero verifica la integridad de la imagen almacenada en la parte alta de la flash
      if( CRC16((uint8_t *)ADDR_FLASH_SECTOR_12, msg->nbytes, 0) != *((uint16_t *)msg->buf) )
      {
        // Error de CRC --> sale sin reprogramar
        flashear = 0;
        error = FLASH_ERROR_CRC_IMAGE;
        break;
      }

      /* Verifica que la nueva imagen sea diferente de la actualmente en ejecución.
         Si son idénticas no vuelve a grabar. */
      for( i=0, Address=ADDR_FLASH_SECTOR_3, data32=(uint32_t *)(ADDR_FLASH_SECTOR_15); i < msg->nbytes; Address+=4, ++data32, i+=4)
      {
        if( *((uint32_t *)Address) != *data32 )
        {
          // Encontró alguna diferencia --> sale para ir a reemplazar el firmware
          break;
        }
      }
      if( i < msg->nbytes )
      {
        LCD_print_raw("ACTUALIZANDO","FIRMWARE");
      }else{
        LCD_print_raw("FIRMWARE","ACTUALIZADO");
        flashear = 0;
        error = 0;
        break;
      }
      /* Luego reemplaza la función flash_pgm() con la nueva, pero antes verifica que
         la nueva flash_pgm() sea diferente de la actual. En caso de ser iguales no
         la reemplaza. */

      for( Address=ADDR_FLASH_SECTOR_1, data32=(uint32_t *)(ADDR_FLASH_SECTOR_13); Address<ADDR_FLASH_SECTOR_2; Address+=4, ++data32)
      {
        if( *((uint32_t *)Address) != *data32 )
        {
          // Encontró alguna diferencia --> sale para ir a reemplazar flash_pgm()
          break;
        }
      }
      if( Address < ADDR_FLASH_SECTOR_2 )
      {
        // Reemplaza flash_pgm() por la nueva versión.

        /* Desbloquea la Flash para habilitar el acceso al registro de control
           Nota: HAL limpia los flags internamente al desbloquear y antes de operar */
        HAL_FLASH_Unlock();

        /* Configurar el borrado masivo de los 12 sectores consecutivos (del 12 al 23) */
        EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
        EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;  // Tensión entre 2.7V y 3.6V
        EraseInitStruct.Sector        = FLASH_SECTOR_1;         // Sector inicial
        EraseInitStruct.NbSectors     = 1;                      // Cantidad de sectores a borrar (1)

        /* Ejecuta el borrado de los sectores en una sola llamada */
        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
        {
          /* Si ocurre un error, 'SectorError' contendrá el número del sector que falló.
             El usuario puede agregar aquí código para manejar este error */
          error = FLASH_ERROR_ERASE_PGM;
        }
        else
        {
          // Copia la nueva función flash_pgm()
          flashear = 1;

          for(Address = ADDR_FLASH_SECTOR_1, data32 = (uint32_t *) ADDR_FLASH_SECTOR_13; Address < ADDR_FLASH_SECTOR_2; Address += 4, ++data32)
          {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, *data32) != HAL_OK)
            {
              error = FLASH_ERROR_WRITE_PGM;
              flashear = 0;
              break;
            }
          }
        }
        /* Bloquea la Flash para deshabilitar el acceso al registro de control de la flash,
         * a fin de proteger el contenido de esta memoria de cualquier operación no deseada
         */
        HAL_FLASH_Lock();
        break;
      }
      else
      {
        // flash_pgm() no tiene cambios, por lo que no hace falta reemplazarla --> ordena reprogramar
        flashear = 1;
      }
      break;

    default:
      error = FLASH_ERROR_INVALID_CODE;
      break;
    }

    rsp = pbuf_alloc(PBUF_TRANSPORT, 64, PBUF_RAM);

    sprintf(rsp->payload, "%s %d %d %x %d", (error ? "NAK" : "ACK"), (int) error, (int) msg->code, (unsigned int) msg->addr, (int) msg->nbytes);

    /* Connect to the remote client */
    udp_connect(upcb, addr, port);

    /* Tell the client that we have accepted it */
    udp_send(upcb, rsp);

    /* free the UDP connection, so we can accept new clients */
    udp_disconnect(upcb);

    /* Free pbuf buffers */
    pbuf_free(p);
    pbuf_free(rsp);

    /* Si corresponde, manda a reprogramar el micro */
    if(flashear)
    {
      flash_pgm();
    }
  }
}

/**
 * @brief  Esta función es la encargada de reemplazar la actual imagen de firmware por
 *         una nueva almacenada en el área superior de la memoria flash.
 *         Está ubicada en la sección ".flash_pgm" de la memoria flash, a partir de la
 *         dirección 0x08040000, en la segunda sección de 16 KB
 * @param  None
 * @return None
 */

#define FLASH_Sector_0     ((uint16_t)0x0000) /*!< Sector Number 0 */
#define FLASH_Sector_1     ((uint16_t)0x0008) /*!< Sector Number 1 */
#define FLASH_Sector_11    ((uint16_t)0x0058) /*!< Sector Number 11 */

void __attribute__((section(".flash_pgm"))) flash_pgm()
{
  uint32_t Address = 0;
  __IO uint32_t * data32;
  uint32_t FLASH_Sector;

  /* Unlock the Flash to enable the flash control register access *************/
  if((FLASH->CR & FLASH_CR_LOCK) != RESET)
  {
    /* Authorize the FLASH Registers access */
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
  }

  /* Disable all interrupts */
  RCC->CIR = 0x00000000;

  // __disable_irq();
  __ASM volatile ("cpsid i");

  /* Clear pending flags (if any) */
  FLASH->SR = FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR;

  for (FLASH_Sector = FLASH_Sector_0; FLASH_Sector <= FLASH_Sector_11; FLASH_Sector += 8)
  {
    /* Saltea el sector de esta sección de código (flash_pgm()) y la siguiente (config) */
    if(FLASH_Sector==FLASH_Sector_1)
      FLASH_Sector += 16;

    /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
     be done by word */

    //status = FLASH_WaitForLastOperation();
    while((FLASH->SR & FLASH_FLAG_BSY) == FLASH_FLAG_BSY);

    /* if the previous operation is completed, proceed to erase the sector */
    FLASH->CR &= CR_PSIZE_MASK;
    FLASH->CR |= FLASH_PSIZE_WORD;
    FLASH->CR &= SECTOR_MASK;
    FLASH->CR |= FLASH_CR_SER | FLASH_Sector;
    FLASH->CR |= FLASH_CR_STRT;

    /* Wait for last operation to be completed */
    //status = FLASH_WaitForLastOperation();
    while((FLASH->SR & FLASH_FLAG_BSY) == FLASH_FLAG_BSY);

    /* if the erase operation is completed, disable the SER Bit */
    FLASH->CR &= (~FLASH_CR_SER);
    FLASH->CR &= SECTOR_MASK;

  }

  /* Program the user Flash area word by word
   (area defined by FLASH_USER_START_ADDR and FLASH_USER_END_ADDR) ***********/

  for (Address = ADDR_FLASH_SECTOR_3, data32 = (uint32_t *) ADDR_FLASH_SECTOR_15; Address < ADDR_FLASH_SECTOR_12;)
  {
    if ( *data32 != 0xffffffff )
    {
      // FLASH_ProgramWord(Address, *data32);

      /* Wait for last operation to be completed */
      //status = FLASH_WaitForLastOperation();
      while((FLASH->SR & FLASH_FLAG_BSY) == FLASH_FLAG_BSY);

      /* if the previous operation is completed, proceed to program the new data */
      FLASH->CR &= CR_PSIZE_MASK;
      FLASH->CR |= FLASH_PSIZE_WORD;
      FLASH->CR |= FLASH_CR_PG;

      *(__IO uint32_t*)Address = *data32;

      /* Wait for last operation to be completed */
      //status = FLASH_WaitForLastOperation();
      while((FLASH->SR & FLASH_FLAG_BSY) == FLASH_FLAG_BSY);

      /* if the program operation is completed, disable the PG Bit */
      FLASH->CR &= (~FLASH_CR_PG);
    }
    Address = Address + 4;
    ++data32;
  }

//  if (FLASH_EraseSector(FLASH_Sector_0, VoltageRange_3) != FLASH_COMPLETE)
//  {
//    /* Error occurred while sector erase.
//     User can add here some code to deal with this error  */
//    while(1);
//  }

  for (Address = ADDR_FLASH_SECTOR_0, data32 = (uint32_t *) ADDR_FLASH_SECTOR_12; Address < ADDR_FLASH_SECTOR_1;)
  {
    if ( *data32 != 0xffffffff )
    {
      // FLASH_ProgramWord(Address, *data32);

      /* Wait for last operation to be completed */
      //status = FLASH_WaitForLastOperation();
      while((FLASH->SR & FLASH_FLAG_BSY) == FLASH_FLAG_BSY);

      /* if the previous operation is completed, proceed to program the new data */
      FLASH->CR &= CR_PSIZE_MASK;
      FLASH->CR |= FLASH_PSIZE_WORD;
      FLASH->CR |= FLASH_CR_PG;

      *(__IO uint32_t*)Address = *data32;

      /* Wait for last operation to be completed */
      //status = FLASH_WaitForLastOperation();
      while((FLASH->SR & FLASH_FLAG_BSY) == FLASH_FLAG_BSY);

      /* if the program operation is completed, disable the PG Bit */
      FLASH->CR &= (~FLASH_CR_PG);
    }
    Address = Address + 4;
    ++data32;
  }

  /* Lock the Flash to disable the flash control register access (recommended
   to protect the FLASH memory against possible unwanted operation) *********/
  //FLASH_Lock();
  /* Set the LOCK Bit to lock the FLASH Registers access */
  FLASH->CR |= FLASH_CR_LOCK;

  // Activa el IWDG para forzar un reset del micro
  IWDG->KR = 0x0000CCCC;
  IWDG->KR = 0x00005555;
  IWDG->PR = 0;

  while(1);  // Se queda esperando que venza el IWDG y reinicie el micro
}

/**
 * @brief Obtiene los parámetros de configuración del módulo desde la flash
 * @param pFeatures: puntero a la estructura donde copiar los parámetros
 * @param mMyCfgIP: puntero a la variable donde almacenar la dirección IP propia
 * @return 1 si OK / 0 si no hay configuración almacenada
 */
int getConfigFromFlash( FeaturesType * pFeatures, uint32_t * pMyCfgIP )
{
  // Busca la última configuración válida
  uint32_t cfgAddr=0, addr;

  // Busca el próximo bloque de 128 libre
  for( addr = ADDR_FLASH_SECTOR_2; addr < ADDR_FLASH_SECTOR_3; addr += 128 )
  {
    if( *((uint32_t *)addr) == 0)
      cfgAddr = addr;

    if( ((addr+128) >= ADDR_FLASH_SECTOR_3) || *((uint32_t *)(addr + 128)) == 0xffffffff )
      break;
  }

  if( cfgAddr )
  {
    // Encontró una configuración grabada. Verifica el CRC y si está OK la copia.
    // Si no, asigna valores por defecto.
    if( !CRC16( (uint8_t *)cfgAddr, 128, 0) )
    {
      // Primero copia las "features"
      *(FeaturesType *)pFeatures = *(FeaturesType *)(cfgAddr+4);

      // Luego la direcci├│n IP
      if( *(uint32_t *)(cfgAddr+4+sizeof(FeaturesType)) != 0xffffffff &&
          *(uint32_t *)(cfgAddr+4+sizeof(FeaturesType)) != 0 )
      {
        *pMyCfgIP = *(uint32_t *)(cfgAddr+4+sizeof(FeaturesType));
      }
      return 1;
    }
  }
  return 0;
}



/**
 * @brief  Activa el IWDG
 * @param  Tiempo inicial en segundos
 * @return None
 */
void startWatchdog(uint32_t t)
{
  IWDG->KR = 0x0000CCCC;
  IWDG->KR = 0x00005555;
  IWDG->PR = 0;
}

/**
 * @brief  Graba nuevos parámetros de configuración del módulo en el sector 2 de la flash.
 * @param  pFeatures: Puntero a la estructura que contiene los nuevos parámetros
 * @param  ip: Dirección IP propia del módulo
 * @return 0: OK / 1: error de borrado / 2: error de grabación / 3: error en verificación
 */
int saveConfigFlash( FeaturesType *pFeatures, uint32_t ip )
{
  int error = 0, i;
  uint32_t Address, addr;
  uint16_t crc;
  uint8_t *data8;

  // Busca el próximo bloque de 128 libre
  for( Address = ADDR_FLASH_SECTOR_2; Address < ADDR_FLASH_SECTOR_3; Address += 128 )
  {
    if( *((uint32_t *)Address) == 0xffffffff )
      break;
  }

  if( Address >= ADDR_FLASH_SECTOR_3 )
  {
    error = deleteConfigFlash();

    if( error )
      return error;
    else
      Address = ADDR_FLASH_SECTOR_2;
  }

  /* Desbloquea la Flash para habilitar el acceso al registro de control */
  HAL_FLASH_Unlock();

  /* Borra todos los flags pendientes, si los hubiera */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

  /* Graba la nueva configuración en el bloque correspondiente */

  __disable_irq();

  // Primero marca el sector como usado
  if ( HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, 0) != HAL_OK )
  {
    error = 2;
  }
  else
  {
    for (i=0, addr = Address+4, data8 = (uint8_t *) pFeatures; i < sizeof(FeaturesType); i += 1)
    {
      if ( HAL_FLASH_Program( FLASH_TYPEPROGRAM_BYTE, addr, *data8) == HAL_OK )
      {
        ++addr;
        ++data8;
      } else
      {
        error = 2;
        break;
      }
    }
  }
  if( !error )
  {
    // A continuación graba la IP propia
    for (i=0, data8 = (uint8_t *) &ip; i < 4; i += 1)
    {
      if ( HAL_FLASH_Program( FLASH_TYPEPROGRAM_BYTE, addr, *data8) == HAL_OK )
      {
        ++addr;
        ++data8;
      } else {
        error = 2;
        break;
      }
    }
    if( !error )
    {
      // Calcula el CRC y lo graba al final del bloque
      crc = CRC16( (uint8_t *)Address, 126, 0);
      if ( HAL_FLASH_Program( FLASH_TYPEPROGRAM_HALFWORD, Address+126, crc) != HAL_OK )
      {
        error = 2;
      }
    }
  }

  __enable_irq();

  /* Bloquea la Flash para deshabilitar el acceso al registro de control de la flash,
   * a fin de proteger el contenido de esta memoria de cualquier operación no deseada
   */
  HAL_FLASH_Lock();

  if( !error )
  {
    if( CRC16((uint8_t *)Address,128,0) )
      error = 3;
  }
  return error;
}

/**
 * @brief  Borra el sector 2 de la flash, destinado a almacenar los par├ímetros de configuraci├│n.
 * @param  None
 * @return 0 si OK; 1 si error
 */
int deleteConfigFlash()
{
  int error = 0;
  FLASH_EraseInitTypeDef EraseInitStruct;
  uint32_t SectorError = 0;

  /* Desbloquea la Flash para habilitar el acceso al registro de control */
  HAL_FLASH_Unlock();

  /* Borra todos los flags pendientes, si los hubiera */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

  EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
  EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // Para VDD entre 2.7V y 3.6V
  EraseInitStruct.Sector       = FLASH_SECTOR_2;         // El sector que quieres borrar
  EraseInitStruct.NbSectors    = 1;                      // Cantidad de sectores a borrar

  /* Se asume que la tensi├│n de alimentaci├│n est├í entre 2.7V y 3.6V,
   * por lo que la operaci├│n puede ser realizada de a palabras de 32 bits */
  if ( HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK )
  {
    /* Error occurred while sector erase.
       User can add here some code to deal with this error  */
    error = 1;
  }
  /* Bloquea la Flash para deshabilitar el acceso al registro de control de la flash,
   * a fin de proteger el contenido de esta memoria de cualquier operaci├│n no deseada
   */
  HAL_FLASH_Lock();

  return error;
}
