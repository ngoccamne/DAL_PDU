#ifndef BSP_POWER_H_
#define BSP_POWER_H_

#include <stdbool.h>

typedef struct
{
    bool pc4;
    bool pc0;
} BSP_PowerFaults;

void BSP_Power_Init(void);
bool BSP_Power_SetTrans(bool enable);
bool BSP_Power_IsTransOn(void);
BSP_PowerFaults BSP_Power_ReadFaults(void);

#endif
