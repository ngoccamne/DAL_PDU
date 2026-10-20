#ifndef BSP_MODBUS_H_
#define BSP_MODBUS_H_

#include <stdbool.h>
#include <stdint.h>

/* FreeMODBUS uses this board port for USART1 and Timer1. The slave address and baud rate
 * come from the configuration module. */
bool BSP_Modbus_InitPins(void);
void BSP_Modbus_DeinitPins(void);

#endif /* BSP_MODBUS_H_ */
