#include "app_xtp.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_power.h"
#include "../M2_BSP/Reset/bsp_reset.h"
#include "../M2_BSP/UART/bsp_uart.h"

/* Same 0xA2, CRC16/XMODEM, ECC, ARQ envelope as the AVR bootloader. Only
 * ping and reboot are needed by xbld_client while the application is running. */
#define XTP_FRAME_MAX 40U
#define XTP_RX_BUDGET 32U
#define XTP_REBOOT_TICKS 10U /* 100 ms at the console task's 10 ms period. */

static uint8_t s_frame[XTP_FRAME_MAX];
static uint8_t s_used;
static uint8_t s_reboot_ticks;

static uint8_t Xtp_Step(uint8_t value)
{
    return (uint8_t)((value >> 1) ^ ((value & 1U) ? 0x8EU : 0U));
}

static uint16_t Xtp_Crc(uint16_t crc, uint8_t value)
{
    uint8_t bit;
    crc ^= (uint16_t)value << 8;
    for (bit = 0U; bit < 8U; bit++)
        crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
    return crc;
}

static void Xtp_Send(uint16_t id, uint8_t sequence, const uint8_t *data, uint8_t length)
{
    uint8_t header[5] = {0xA2U, (uint8_t)id, (uint8_t)(id >> 8), sequence, length};
    uint8_t parity[2] = {0U, 0U};
    uint16_t crc = 0U;
    uint8_t i;
    BSP_UART_Write(UART_NUMBER_UART0, 0xC0U);
    BSP_UART_Write(UART_NUMBER_UART0, 0xDEU);
    for (i = 0U; i < 5U; i++)
    {
        crc = Xtp_Crc(crc, header[i]);
        BSP_UART_Write(UART_NUMBER_UART0, header[i]);
    }
    for (i = 0U; i < length; i++)
    {
        parity[0] ^= data[i];
        parity[1] = Xtp_Step((uint8_t)(parity[1] ^ data[i]));
        crc = Xtp_Crc(crc, data[i]);
        BSP_UART_Write(UART_NUMBER_UART0, data[i]);
    }
    for (i = 0U; i < 2U; i++)
    {
        crc = Xtp_Crc(crc, parity[i]);
        BSP_UART_Write(UART_NUMBER_UART0, parity[i]);
    }
    BSP_UART_Write(UART_NUMBER_UART0, (uint8_t)crc);
    BSP_UART_Write(UART_NUMBER_UART0, (uint8_t)(crc >> 8));
    BSP_UART_Write(UART_NUMBER_UART0, 0xDAU);
    BSP_UART_Write(UART_NUMBER_UART0, 0xEDU);
}

static void Xtp_HandleFrame(void)
{
    uint8_t length = s_frame[6];
    uint8_t sequence = (uint8_t)(s_frame[5] & 0x7FU);
    uint8_t i;
    uint16_t crc = 0U;
    uint16_t expected = (uint16_t)s_frame[9U + length] |
                        ((uint16_t)s_frame[10U + length] << 8);
    uint8_t parity[2] = {0U, 0U};
    uint8_t error;
    uint8_t syndrome;
    uint8_t signature;
    uint8_t distance;
    const uint8_t *request = s_frame + 7U;
    uint8_t ack[1] = {sequence};
    uint8_t reply[17] = {1U, 0U, 0U, 0U, 1U, 0U};
    uint8_t reply_length;

    if (s_frame[11U + length] != 0xDAU || s_frame[12U + length] != 0xEDU) return;
    for (i = 2U; i < (uint8_t)(9U + length); i++) crc = Xtp_Crc(crc, s_frame[i]);
    for (i = 0U; i < length; i++)
    {
        parity[0] ^= request[i];
        parity[1] = Xtp_Step((uint8_t)(parity[1] ^ request[i]));
    }
    if (crc != expected)
    {
        /* Match the bootloader: repair one payload byte only when the original
         * frame CRC also validates the repaired data. */
        error = (uint8_t)(parity[0] ^ s_frame[7U + length]);
        syndrome = (uint8_t)(parity[1] ^ s_frame[8U + length]);
        signature = error;
        if (error == 0U || syndrome == 0U) return;
        for (distance = 1U; distance <= length; distance++)
        {
            signature = Xtp_Step(signature);
            if (signature == syndrome)
            {
                s_frame[7U + length - distance] ^= error;
                crc = 0U;
                for (i = 2U; i < (uint8_t)(9U + length); i++)
                    crc = Xtp_Crc(crc, s_frame[i]);
                if (crc != expected) return;
                break;
            }
        }
        if (distance > length) return;
    }
    if (s_frame[3] != 0U || s_frame[4] != 1U || length < 3U || request[0] != 0U)
        return;

    Xtp_Send(0xFFFEU, 0U, ack, 1U);
    reply[1] = request[1];
    reply[2] = request[2];
    if (request[1] == 0x02U && length == 9U &&
        request[3] == 'p' && request[4] == 'i' && request[5] == 'n' &&
        request[6] == 'g' && request[7] == ' ' && request[8] == '0')
    {
        /* TLV: mode (UTF-8 string) = app. */
        reply[6] = 4U; reply[7] = 'm'; reply[8] = 'o'; reply[9] = 'd';
        reply[10] = 'e'; reply[11] = 9U; reply[12] = 3U; reply[13] = 0U;
        reply[14] = 'a'; reply[15] = 'p'; reply[16] = 'p';
        reply_length = 17U;
    }
    else if (request[1] == 0x04U && length == 11U &&
             request[3] == 'r' && request[4] == 'e' && request[5] == 'b' &&
             request[6] == 'o' && request[7] == 'o' && request[8] == 't' &&
             request[9] == ' ' && request[10] == '0')
    {
        reply_length = 6U;
        AppPower_TurnOffOutputs();
        s_reboot_ticks = XTP_REBOOT_TICKS;
    }
    else
    {
        reply[3] = 2U; /* XCLI_STATUS_BAD_ARG */
        reply_length = 6U;
    }
    Xtp_Send(0x0101U, 0U, reply, reply_length);
}

void AppXtp_Init(void)
{
    s_used = 0U;
    s_reboot_ticks = 0U;
}

void AppXtp_Run(void)
{
    uint8_t byte;
    uint8_t budget = XTP_RX_BUDGET;
    if (s_reboot_ticks != 0U && --s_reboot_ticks == 0U)
        BSP_Reset_Bootloader();
    while (budget-- != 0U && BSP_UART_Read(UART_NUMBER_UART0, &byte))
    {
        if (s_used == 0U)
        {
            if (byte == 0xC0U) s_frame[s_used++] = byte;
            continue;
        }
        if (s_used == 1U && byte != 0xDEU)
        {
            s_used = (byte == 0xC0U) ? 1U : 0U;
            continue;
        }
        s_frame[s_used++] = byte;
        if (s_used == 3U && byte != 0xA2U) { s_used = 0U; continue; }
        if (s_used >= 7U)
        {
            uint16_t total = (uint16_t)s_frame[6] + 13U;
            if (total > XTP_FRAME_MAX) { s_used = 0U; continue; }
            if (s_used == total)
            {
                Xtp_HandleFrame();
                s_used = 0U;
            }
        }
    }
}
