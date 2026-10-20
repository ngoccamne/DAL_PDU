#ifndef BSP_TPS55287_H_
#define BSP_TPS55287_H_

#include <stdbool.h>
#include <stdint.h>
#include "../../M3_Driver/Devices/TPS55287/tps55287.h"

/* Last observed TPS55287 values, owned by this module. Validity flags distinguish unknown data.
 * Written only from the main loop. */
typedef struct
{
    uint8_t address; /* Zero means no device has been detected. */
    bool state_valid;
    uint8_t mode;
    uint8_t feedback;
    uint8_t status;
    uint16_t reference_code;
    bool voltage_valid;
    uint16_t voltage_mv;
    /* Commanded setpoint from the console or Modbus. Valid after a successful command since reset. */
    bool setpoint_valid;
    uint16_t setpoint_mv;
    bool commanded_enable;
} BSP_TPS55287_Status;

/* Returns a copy of the record. */
BSP_TPS55287_Status BSP_TPS55287_GetStatus(void);
/* Clears the record. Host tests use it to start from a known state. */
void BSP_TPS55287_ResetStatus(void);

/* Detect U5 on either address selected by its MODE pin. Does not turn on VOUT. */
bool BSP_TPS55287_Detect(void);
uint8_t BSP_TPS55287_Address(void);
bool BSP_TPS55287_ReadState(TPS55287_State *state);
bool BSP_TPS55287_SetVoltageMv(uint16_t millivolts);
bool BSP_TPS55287_GetVoltageMv(uint16_t *millivolts);
/* Selects and verifies internal feedback on the 20 V scale (register 0x04 = 0x03). */
bool BSP_TPS55287_SetInternalFeedback(void);
/* Read only the enable bit; a full state read clears latched fault flags. */
bool BSP_TPS55287_IsEnabled(bool *enabled);
/* Enabling requires TRANS on and verifies internal feedback first. */
bool BSP_TPS55287_SetEnabled(bool enabled);

#endif /* BSP_TPS55287_H_ */
