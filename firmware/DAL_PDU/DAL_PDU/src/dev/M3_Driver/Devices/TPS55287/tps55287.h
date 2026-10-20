#ifndef TPS55287_H_
#define TPS55287_H_

#include <stdbool.h>
#include <stdint.h>

#define TPS55287_ADDR_MODE_HIGH 0x74U
#define TPS55287_ADDR_MODE_LOW  0x75U
#define TPS55287_INTERNAL_FEEDBACK_20V 0x03U
#define TPS55287_MIN_VOUT_MV 800U
#define TPS55287_MAX_VOUT_MV 20000U

typedef struct
{
    uint8_t address;
} TPS55287_Device;

typedef struct
{
    uint8_t mode;
    uint8_t feedback;
    uint8_t status; /* Reading this register clears latched SCP/OCP/OVP bits. */
    uint16_t reference_code;
} TPS55287_State;

/* The board supplies these single-register I2C transfers. */
bool TPS55287_BusRead(uint8_t address, uint8_t reg, uint8_t *value);
bool TPS55287_BusWrite(uint8_t address, uint8_t reg, uint8_t value);

bool TPS55287_ReadState(const TPS55287_Device *device, TPS55287_State *state);
bool TPS55287_SetEnabled(const TPS55287_Device *device, bool enabled);
/* Uses internal feedback on the 20 V scale (VOUT_FS = 0x03). */
bool TPS55287_SetVoltageMv(const TPS55287_Device *device, uint16_t millivolts);
bool TPS55287_GetVoltageMv(const TPS55287_Device *device, uint16_t *millivolts);
/* Selects internal feedback and its 20 V scale, then reads back register 0x04. */
bool TPS55287_SetInternalFeedback(const TPS55287_Device *device);

#endif /* TPS55287_H_ */
