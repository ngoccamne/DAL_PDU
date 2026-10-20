#ifndef BSP_PAC1934_H_
#define BSP_PAC1934_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../M3_Driver/Devices/PAC1934/pac1934.h"

/* Finds a PAC1934 by product ID across its 0x10..0x1f address range. */
bool BSP_PAC1934_Detect(void);
uint8_t BSP_PAC1934_Address(void);
bool BSP_PAC1934_ReadSnapshot(PAC1934_Sample *sample);

#endif /* BSP_PAC1934_H_ */
