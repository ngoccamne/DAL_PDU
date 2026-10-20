#ifndef PAC1934_H_
#define PAC1934_H_

#include <stdbool.h>
#include <stdint.h>

#define PAC1934_CHANNEL_COUNT 4U
#define PAC1934_PRODUCT_ID    0x5BU
#define PAC1934_REG_PRODUCT_ID 0xFDU
#define PAC1934_CMD_REFRESH_V 0x1FU

typedef struct
{
    uint16_t voltage_mv[PAC1934_CHANNEL_COUNT];
    uint32_t current_ma[PAC1934_CHANNEL_COUNT];
    uint32_t power_mw[PAC1934_CHANNEL_COUNT];
} PAC1934_Sample;

/* Supplied by the board binding. ReadBlock returns bytes in bus order. */
bool PAC1934_BusRead(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length);
bool PAC1934_BusCommand(uint8_t address, uint8_t command);

bool PAC1934_CheckId(uint8_t address);
bool PAC1934_Refresh(uint8_t address);
/* Call at least 1 ms after Refresh; all channels must use the same shunt. */
bool PAC1934_ReadAll(uint8_t address, uint16_t shunt_mohm, PAC1934_Sample *sample);

#endif /* PAC1934_H_ */
