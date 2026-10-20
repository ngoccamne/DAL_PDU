#include "app_temperature.h"

#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/NTC/bsp_ntc.h"

static void AppTemperature_Publish(void)
{
    SystemData_TemperatureStatus status;
    status.tenths_c = 0;
    status.valid = BSP_NTC_ReadTenthsC(&status.tenths_c);
    SystemData_SetTemperatureStatus(status);
}

void AppTemperature_Init(void)
{
    (void)BSP_NTC_Init();
    AppTemperature_Publish();
}

void AppTemperature_Update(void)
{
    AppTemperature_Publish();
}
