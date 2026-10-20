#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_power_monitor.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_pac1934.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/PAC1934/bsp_pac1934.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/TPS55287/bsp_tps55287.h"

void AppSetpoint_Update(void)
{
}

void AppLimits_Update(void)
{
}

static bool bus_fault;
static uint8_t bus_fault_address;
static unsigned regulator_faults;
static bool trans_on = true;

bool BSP_Power_IsTransOn(void)
{
    return trans_on;
}

bool BSP_I2C_BusFault(void)
{
    return bus_fault;
}

uint8_t BSP_I2C_FaultAddress(void)
{
    return bus_fault ? bus_fault_address : 0U;
}

uint8_t BSP_TPS55287_Address(void)
{
    return 0x74U;
}

void AppMode_SetBusFault(bool fault)
{
    (void)fault;
}

bool AppEvent_Log(uint8_t severity, uint8_t code, int16_t value)
{
    (void)severity;
    (void)code;
    (void)value;
    return true;
}

void AppMode_SetPacUnreadable(bool unreadable)
{
    (void)unreadable;
}

static bool regulator_monitored;

bool AppMode_RegulatorMonitored(void)
{
    return regulator_monitored;
}

void AppMode_SetRegulatorFault(void)
{
    regulator_faults++;
}

bool BSP_TPS55287_ReadState(TPS55287_State *state)
{
    if (state != 0) state->status = 0U;
    return true;
}

static unsigned failures;
static unsigned refresh_count;
static bool fail_measurement;
static char output[512];

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); failures++; } } while (0)

bool BSP_I2C_ReadBlock(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length)
{
    if (address != 0x1FU) return false;
    if (reg == PAC1934_REG_PRODUCT_ID && length == 1U)
    {
        data[0] = PAC1934_PRODUCT_ID;
        return true;
    }
    if ((reg == 0x22U || reg == 0x23U) && length == 1U)
    {
        data[0] = 0U;
        return true;
    }
    if (fail_measurement || length == 0U) return false;
    if (reg >= 0x0FU && reg <= 0x12U && length == 2U)
    {
        data[0] = 0x28U; data[1] = 0x00U; /* 5 V */
        return true;
    }
    if (reg >= 0x13U && reg <= 0x16U && length == 2U)
    {
        data[0] = 0x19U; data[1] = 0x9AU; /* 1 A with a 10 mOhm shunt */
        return true;
    }
    if (reg >= 0x17U && reg <= 0x1AU && length == 4U)
    {
        data[0] = 0x04U; data[1] = 0U; data[2] = 0U; data[3] = 0U; /* 5 W */
        return true;
    }
    return false;
}

bool BSP_I2C_SendCommand(uint8_t address, uint8_t command)
{
    if (address != 0x1FU || command != PAC1934_CMD_REFRESH_V) return false;
    refresh_count++;
    return true;
}

void Command_OutputString(CommandContext *context, const char *text)
{
    (void)context;
    if (strlen(output) + strlen(text) < sizeof(output)) strcat(output, text);
}

static void test_bus_timeout_owner_sets_error_or_warning(void)
{
    unsigned before;
    bus_fault = true;
    bus_fault_address = 0x1FU;
    regulator_monitored = true;
    before = regulator_faults;
    AppPowerMonitor_Update();
    CHECK(regulator_faults == before); /* Other device: WARNING only. */
    bus_fault_address = 0x74U;
    regulator_monitored = false;
    AppPowerMonitor_Update();
    CHECK(regulator_faults == before); /* MANUAL: WARNING cause only. */
    regulator_monitored = true;
    AppPowerMonitor_Update();
    CHECK(regulator_faults > before); /* NORMAL or WARNING: ERROR. */
    before = regulator_faults;
    trans_on = false;
    AppPowerMonitor_Update();
    CHECK(regulator_faults == before); /* TPS55287 is off at idle; a bus fault then isn't its own. */
    trans_on = true;
    bus_fault = false;
    regulator_monitored = false;
    refresh_count = 0U;
}

int main(void)
{
    SystemData_PowerMonitorStatus status;
    test_bus_timeout_owner_sets_error_or_warning();
    char *get[] = {"pac1934", "get"};
    char *refresh[] = {"pac1934", "refresh"};
    char *bad[] = {"pac1934", "bad"};
    SystemData_Init();
    status = SystemData_GetPowerMonitorStatus();
    CHECK(!status.valid && status.address == 0U);
    output[0] = '\0';
    CHECK(Cmd_pac1934(0, 2, get) == CMDLINE_OK);
    CHECK(strcmp(output, "PAC1934 unavailable\r\n") == 0);
    AppPowerMonitor_Update();
    status = SystemData_GetPowerMonitorStatus();
    CHECK(status.valid && status.address == 0x1FU);
    CHECK(refresh_count == 1U);
    CHECK(status.voltage_mv[0] == 5000U && status.current_ma[0] == 1000U);
    CHECK(status.power_mw[0] == 5000UL);
    CHECK(status.voltage_mv[3] == 5000U && status.power_mw[3] == 5000UL);
    output[0] = '\0';
    CHECK(Cmd_pac1934(0, 2, get) == CMDLINE_OK);
    CHECK(strstr(output, "PAC1934 addr=0x1f\r\n") != 0);
    CHECK(strstr(output, "VOUT 5000 mV 1000 mA 5000 mW\r\n") != 0);
    CHECK(strstr(output, "+5 5000 mV 1000 mA 5000 mW\r\n") != 0);
    CHECK(Cmd_pac1934(0, 2, bad) == CMDLINE_INVALID_ARG);
    output[0] = '\0';
    CHECK(Cmd_pac1934(0, 2, refresh) == CMDLINE_OK);
    CHECK(refresh_count == 2U);
    fail_measurement = true;
    AppPowerMonitor_Update();
    status = SystemData_GetPowerMonitorStatus();
    CHECK(!status.valid && status.address == 0U);
    output[0] = '\0';
    CHECK(Cmd_pac1934(0, 2, get) == CMDLINE_OK);
    CHECK(strcmp(output, "PAC1934 unavailable\r\n") == 0);
    if (failures == 0U) puts("PAC1934 tests passed");
    return failures == 0U ? 0 : 1;
}
