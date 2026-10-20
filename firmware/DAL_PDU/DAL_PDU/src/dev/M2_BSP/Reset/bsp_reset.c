#include "bsp_reset.h"

#ifdef __AVR__
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include "../dal_pdu_board.h"
#include <util/delay.h>
#define REQUEST_SECTION __attribute__((section(".noinit")))
#else
#define REQUEST_SECTION
#endif

#define REQUEST_MAGIC 0xD2U
#define REQUEST_KEY 0x3CU
/* The console reply must leave the UART before the MCU restarts. */
#define REQUEST_DRAIN_MS 100

/* In RAM, so it survives the watchdog reset. Written last, so a reset mid-write reads back as no request. */
typedef struct
{
    uint8_t magic;
    uint8_t check;
} RequestRecord;

static RequestRecord s_request REQUEST_SECTION;

uint8_t BSP_Reset_CaptureFlags(void)
{
#ifdef __AVR__
    uint8_t flags = MCUCSR;
    MCUCSR = 0U;
    /* A watchdog reset leaves WDE set, which would reset the MCU again every 15 ms. */
    wdt_disable();
    return flags;
#else
    return 0U;
#endif
}

bool BSP_Reset_TakeSoftwareRequest(void)
{
    bool requested = s_request.magic == REQUEST_MAGIC && s_request.check == REQUEST_KEY;
    s_request.magic = 0U;
    return requested;
}

void BSP_Reset_Software(void)
{
    s_request.check = REQUEST_KEY;
    s_request.magic = REQUEST_MAGIC;
#ifdef __AVR__
    _delay_ms(REQUEST_DRAIN_MS);
    wdt_enable(WDTO_15MS);
    for (;;)
    {
    }
#endif
}

void BSP_Reset_Bootloader(void)
{
#ifdef __AVR__
    /* Commit byte is last, matching AVRBootLoader/include/avr_boot_request.h. */
    eeprom_update_byte((uint8_t *)(uintptr_t)0x0FC0U, 0U);
    eeprom_update_byte((uint8_t *)(uintptr_t)0x0FC1U, 0x5AU);
    eeprom_update_byte((uint8_t *)(uintptr_t)0x0FC0U, 0xA5U);
    eeprom_busy_wait();
    wdt_enable(WDTO_15MS);
    for (;;) { }
#endif
}
