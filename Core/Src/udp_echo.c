/*
 * udp_echo.c
 *
 *  Created on: Sep 27, 2026
 *      Author: guillermo
 */

#include "udp_echo.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include <string.h>
#include "lcd.h"

/* Handle del PCB UDP */
static struct udp_pcb *echo_pcb = NULL;

/* ─────────────────────────────────────────────
 * Callback — se llama automáticamente cuando
 * llega un datagrama UDP al puerto de escucha
 * ───────────────────────────────────────────── */
static void udp_echo_recv(void *arg,
                          struct udp_pcb *pcb,
                          struct pbuf *p,
                          const ip_addr_t *addr,
                          u16_t port)
{
    if (p == NULL) return;

    /* Enviar el mismo pbuf de vuelta al remitente */
    udp_sendto(pcb, p, addr, port);

    LCD_print("ECHO",(char *)p->payload);

    /* Liberar el pbuf — siempre obligatorio */
    pbuf_free(p);
}

/* ─────────────────────────────────────────────
 * Inicializar el servidor UDP echo
 * Llamar una vez después de MX_LWIP_Init()
 * ───────────────────────────────────────────── */
void UDP_Echo_Init(void)
{
    err_t err;

    /* Crear PCB UDP */
    echo_pcb = udp_new();
    if (echo_pcb == NULL) return;

    /* Vincular al puerto de escucha en todas las interfaces */
    err = udp_bind(echo_pcb, IP_ADDR_ANY, UDP_ECHO_PORT);
    if (err != ERR_OK)
    {
        udp_remove(echo_pcb);
        echo_pcb = NULL;
        return;
    }

    /* Registrar callback de recepción */
    udp_recv(echo_pcb, udp_echo_recv, NULL);
}

/* ─────────────────────────────────────────────
 * Proceso — por ahora vacío porque LWIP maneja
 * todo por callbacks. Reservado para lógica futura
 * ───────────────────────────────────────────── */
void UDP_Echo_Process(void)
{
    /* El stack LWIP llama udp_echo_recv() automáticamente
     * cuando llega un paquete — no hay nada que hacer acá */
}

