/*
 * ds2401.c
 *
 *  Created on: Sep 26, 2026
 *      Author: guillermo
 */

#include "ds2401.h"

/* ── Ajustar según el pin usado en tu placa ── */
#define OW_PORT     DS2401_PIN_GPIO_Port
#define OW_PIN      DS2401_PIN_Pin

/* ─────────────────────────────────────────────
 * Delays precisos para protocolo 1-Wire
 * Con HCLK = 168 MHz
 * ───────────────────────────────────────────── */
static void ow_delay_us(uint32_t us)
{
    /* Usar DWT cycle counter para delays precisos en microsegundos */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

static void ow_init_dwt(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ─────────────────────────────────────────────
 * Primitivas 1-Wire
 * ───────────────────────────────────────────── */

/* Poner el bus en bajo */
static void ow_low(void)
{
    HAL_GPIO_WritePin(OW_PORT, OW_PIN, GPIO_PIN_RESET);
}

/* Soltar el bus (pull-up externo lo lleva a alto) */
static void ow_release(void)
{
    HAL_GPIO_WritePin(OW_PORT, OW_PIN, GPIO_PIN_SET);
}

/* Leer el estado del bus */
static uint8_t ow_read_bit(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(OW_PORT, OW_PIN);
}

/* Reset y detección de presencia
 * Retorna 1 si el DS2401 responde, 0 si no hay dispositivo */
static uint8_t ow_reset(void)
{
    uint8_t presence;

    /* Pulso de reset: low por 480 us */
    ow_low();
    ow_delay_us(480);

    /* Soltar bus y esperar */
    ow_release();
    ow_delay_us(70);

    /* Leer pulso de presencia */
    presence = (ow_read_bit() == GPIO_PIN_RESET) ? 1 : 0;

    /* Esperar fin del time slot */
    ow_delay_us(410);

    return presence;
}

/* Escribir un bit */
static void ow_write_bit(uint8_t bit)
{
    if (bit)
    {
        /* Write 1: low por 6 us, soltar por 64 us */
        ow_low();
        ow_delay_us(6);
        ow_release();
        ow_delay_us(64);
    }
    else
    {
        /* Write 0: low por 60 us, soltar por 10 us */
        ow_low();
        ow_delay_us(60);
        ow_release();
        ow_delay_us(10);
    }
}

/* Escribir un byte */
static void ow_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++)
    {
        ow_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

/* Leer un bit */
static uint8_t ow_read_bit_val(void)
{
    uint8_t bit;

    /* Iniciar time slot: low por 6 us */
    ow_low();
    ow_delay_us(6);

    /* Soltar bus y samplear después de 9 us */
    ow_release();
    ow_delay_us(9);

    bit = (ow_read_bit() == GPIO_PIN_SET) ? 1 : 0;

    /* Completar time slot */
    ow_delay_us(55);

    return bit;
}

/* Leer un byte */
static uint8_t ow_read_byte(void)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++)
    {
        byte |= (ow_read_bit_val() << i);
    }
    return byte;
}

/* ─────────────────────────────────────────────
 * CRC8 para verificar integridad de la lectura
 * Polinomio Dallas/Maxim: x^8 + x^5 + x^4 + 1
 * ───────────────────────────────────────────── */
static uint8_t ow_crc8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

/* ─────────────────────────────────────────────
 * Función principal: leer MAC desde DS2401
 * ───────────────────────────────────────────── */
DS2401_Status DS2401_ReadMAC(uint8_t *mac_addr)
{
    uint8_t rom[8];   /* 8 bytes: familia + 6 serie + CRC */

    /* Inicializar DWT para delays precisos */
    ow_init_dwt();

    /* Reset y verificar presencia */
    if (!ow_reset())
        return DS2401_ERROR;   /* DS2401 no responde */

    /* Comando Read ROM (0x33) — válido cuando hay un solo dispositivo */
    ow_write_byte(0x33);

    /* Leer 8 bytes */
    for (int i = 0; i < 8; i++)
        rom[i] = ow_read_byte();

    /* Verificar CRC */
    if (ow_crc8(rom, 7) != rom[7])
        return DS2401_ERROR;   /* CRC incorrecto — lectura corrupta */

    /* Verificar código de familia */
    if (rom[0] != 0x01)
        return DS2401_ERROR;   /* no es un DS2401 */

    /* Extraer 6 bytes de número de serie → MAC address
     * rom[1] es el byte menos significativo del serie */
    mac_addr[0] = rom[6];
    mac_addr[1] = rom[5];
    mac_addr[2] = rom[4];
    mac_addr[3] = rom[3];
    mac_addr[4] = rom[2];
    mac_addr[5] = rom[1];

    /* Forzar bit U/L = 0 (MAC unicast) y bit I/G = 0 (individual) */
    mac_addr[0] &= 0xFE;   /* bit 0 = 0: unicast */
    mac_addr[0] &= 0xFC;   /* bit 1 = 0: universally administered */

    return DS2401_OK;
}

