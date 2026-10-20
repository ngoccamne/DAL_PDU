#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_tps55287.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/TPS55287/bsp_tps55287.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Power/bsp_power.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"

static uint8_t registers[8] = {0xA4U, 0x01U, 0xE4U, 0x01U, 0x03U, 0xE0U, 0x20U, 0xA1U};
static uint8_t address = TPS55287_ADDR_MODE_LOW;
static uint8_t writes[8];
static uint8_t write_count;
static bool feedback_write_sticks = true;
static bool trans_on;
static bool fault;
static unsigned failures;
static char output[256];

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); failures++; } } while (0)

bool BSP_I2C_ReadRegister(uint8_t addr, uint8_t reg, uint8_t *value)
{
    if (addr != address || reg >= sizeof(registers)) return false;
    *value = registers[reg];
    if (reg == 7U) registers[7] &= 0x1FU;
    return true;
}

bool BSP_I2C_WriteRegister(uint8_t addr, uint8_t reg, uint8_t value)
{
    if (addr != address || reg >= sizeof(registers)) return false;
    if (write_count < sizeof(writes)) writes[write_count++] = reg;
    if (reg != 4U || feedback_write_sticks) registers[reg] = value;
    return true;
}

BSP_PowerFaults BSP_Power_ReadFaults(void)
{
    BSP_PowerFaults result = {fault, false};
    return result;
}

bool BSP_Power_IsTransOn(void) { return trans_on; }

bool AppPower_EnsureRegulatorOn(void)
{
    if (trans_on) return true;
    trans_on = true;
    return true;
}

void Command_OutputString(CommandContext *context, const char *text)
{
    (void)context;
    if (strlen(output) + strlen(text) < sizeof(output)) strcat(output, text);
}

static int Run(int argc, char *argv[])
{
    output[0] = '\0';
    return Cmd_tps55287(0, argc, argv);
}

int main(void)
{
    TPS55287_State state;
    BSP_TPS55287_Status data;
    uint16_t millivolts;
    char *set_5v[] = {"tps55287", "set", "5000"};
    char *set_20v[] = {"tps55287", "set", "20000"};
    char *set_12v[] = {"tps55287", "set", "12000"};
    char *set_bad[] = {"tps55287", "set", "25000"};
    char *set_low[] = {"tps55287", "set", "799"};
    char *on[] = {"tps55287", "on"};
    char *off[] = {"tps55287", "off"};
    char *get[] = {"tps55287", "get"};
    char *status[] = {"tps55287", "status"};

    SystemData_Init();
    BSP_TPS55287_ResetStatus();
    data = BSP_TPS55287_GetStatus();
    CHECK(data.address == 0U && !data.state_valid && !data.voltage_valid);
    CHECK(BSP_TPS55287_Detect());
    CHECK(BSP_TPS55287_Address() == 0x75U);
    CHECK(BSP_TPS55287_GetStatus().address == 0x75U);
    CHECK(BSP_TPS55287_GetVoltageMv(&millivolts));
    CHECK(millivolts == 5000U);
    trans_on = true;
    CHECK(!BSP_TPS55287_SetEnabled(true)); /* Readable POR code is not a commanded setpoint. */
    trans_on = false;
    data = BSP_TPS55287_GetStatus();
    CHECK(data.voltage_valid && data.voltage_mv == millivolts);
    CHECK(Run(3, set_bad) == CMDLINE_INVALID_ARG);
    CHECK(Run(3, set_low) == CMDLINE_INVALID_ARG);

    /* "set" powers TRANS on by itself now; it starts off here. */
    CHECK(!trans_on);
    CHECK(Run(3, set_5v) == CMDLINE_OK);
    CHECK(trans_on);
    CHECK(write_count == 2U && writes[0] == 0U && writes[1] == 1U);
    CHECK(registers[4] == TPS55287_INTERNAL_FEEDBACK_20V);
    CHECK(Run(2, get) == CMDLINE_OK && strstr(output, "5000 mV") != 0);
    CHECK(Run(3, set_20v) == CMDLINE_OK && strstr(output, "set OK") != 0);
    CHECK(registers[0] == 0x80U && registers[1] == 0x07U); /* REF = 0x780. */
    CHECK(BSP_TPS55287_GetVoltageMv(&millivolts) && millivolts == 20000U);
    CHECK(!BSP_TPS55287_SetVoltageMv(20001U));
    CHECK(BSP_TPS55287_SetVoltageMv(800U));
    CHECK(registers[0] == 0U && registers[1] == 0U);
    CHECK(BSP_TPS55287_GetVoltageMv(&millivolts) && millivolts == 800U);
    CHECK(BSP_TPS55287_SetVoltageMv(805U));
    CHECK(BSP_TPS55287_GetVoltageMv(&millivolts) && millivolts == 810U);
    CHECK(Run(3, set_12v) == CMDLINE_OK && strstr(output, "set OK") != 0);
    CHECK(BSP_TPS55287_GetVoltageMv(&millivolts) && millivolts == 12000U);

    /* "on" powers TRANS on by itself too, independently of "set" having already done it. */
    trans_on = false;
    CHECK(Run(2, on) == CMDLINE_OK && registers[6] == 0xA0U);
    CHECK(trans_on);
    fault = true;
    CHECK(Run(2, on) == CMDLINE_OK && strstr(output, "TPS55287 on") != 0);
    fault = false;
    CHECK(Run(2, on) == CMDLINE_OK && registers[6] == 0xA0U);
    {
        bool enabled = false;
        CHECK(BSP_TPS55287_IsEnabled(&enabled) && enabled);
        CHECK(registers[7] == 0xA1U); /* Reading enable must not clear fault flags. */
    }
    CHECK(Run(2, status) == CMDLINE_OK && strstr(output, "SCP") != 0 && strstr(output, "OVP") != 0);
    CHECK(registers[7] == 0x01U);
    data = BSP_TPS55287_GetStatus();
    CHECK(data.state_valid && data.mode == 0xA0U && data.status == 0xA1U);
    CHECK(Run(2, off) == CMDLINE_OK && registers[6] == 0x20U);

    /* A raw change to external feedback is corrected before OE is set; reapply the
     * known 12 V setpoint so the old reference code cannot produce another VOUT. */
    registers[4] = 0x80U;
    write_count = 0U;
    CHECK(Run(2, on) == CMDLINE_OK && strstr(output, "TPS55287 on") != 0);
    CHECK(registers[4] == 0x03U && registers[6] == 0xA0U);
    CHECK(write_count == 4U && writes[0] == 4U && writes[1] == 0U &&
          writes[2] == 1U && writes[3] == 6U);
    CHECK(Run(2, off) == CMDLINE_OK && registers[6] == 0x20U);

    registers[4] = 0x80U;
    feedback_write_sticks = false;
    CHECK(Run(2, on) == CMDLINE_OK && strstr(output, "failed") != 0);
    CHECK(registers[6] == 0x20U); /* Verification failure must leave OE off. */
    registers[6] = 0xA0U;
    write_count = 0U;
    CHECK(Run(2, on) == CMDLINE_OK && strstr(output, "failed") != 0);
    CHECK(registers[6] == 0x20U); /* Also shut down an already-enabled wrong mode. */
    CHECK(write_count == 2U && writes[0] == 6U && writes[1] == 4U);
    feedback_write_sticks = true;

    /* A set command corrects the mode, programs the internal-scale reference,
     * then restores an output that had been enabled. */
    registers[6] = 0xA0U;
    write_count = 0U;
    CHECK(Run(3, set_5v) == CMDLINE_OK && strstr(output, "set OK") != 0);
    CHECK(registers[4] == 0x03U && registers[6] == 0xA0U);
    CHECK(write_count == 5U && writes[0] == 6U && writes[1] == 4U &&
          writes[2] == 0U && writes[3] == 1U && writes[4] == 6U);
    CHECK(Run(2, off) == CMDLINE_OK && registers[6] == 0x20U);

    /* A previously unknown reference code in external mode must not be enabled
     * simply by correcting the feedback register. */
    BSP_TPS55287_ResetStatus();
    CHECK(BSP_TPS55287_Detect());
    registers[4] = 0x80U;
    write_count = 0U;
    CHECK(Run(2, on) == CMDLINE_OK && strstr(output, "failed") != 0);
    CHECK(registers[6] == 0x20U);
    CHECK(registers[4] == 0x03U && write_count == 1U && writes[0] == 4U);
    CHECK(Run(3, set_5v) == CMDLINE_OK && strstr(output, "set OK") != 0);
    CHECK(Run(2, on) == CMDLINE_OK && strstr(output, "TPS55287 on") != 0);
    CHECK(Run(2, off) == CMDLINE_OK && registers[6] == 0x20U);

    /* Readback refuses another scale until the BSP restores internal 20 V mode. */
    registers[4] = 0x83U;
    CHECK(!BSP_TPS55287_GetVoltageMv(&millivolts));
    CHECK(Run(3, set_5v) == CMDLINE_OK && strstr(output, "set OK") != 0);
    CHECK(BSP_TPS55287_GetVoltageMv(&millivolts) && millivolts == 5000U);
    CHECK(BSP_TPS55287_GetStatus().voltage_valid);
    CHECK(BSP_TPS55287_ReadState(&state) && state.feedback == 0x03U);
    CHECK(BSP_TPS55287_GetStatus().feedback == 0x03U);

    /* Internal mode and its 20 V scale are written and verified explicitly. */
    registers[4] = 0x80U;
    {
        TPS55287_Device device;
        device.address = address;
        CHECK(TPS55287_SetInternalFeedback(&device));
        CHECK(registers[4] == 0x03U);
    }

    /* MODE high selects the other 7-bit address and the same internal scale. */
    address = TPS55287_ADDR_MODE_HIGH;
    registers[4] = 0x01U;
    CHECK(BSP_TPS55287_Detect());
    CHECK(BSP_TPS55287_Address() == 0x74U);
    data = BSP_TPS55287_GetStatus();
    CHECK(data.address == 0x74U && !data.state_valid && !data.voltage_valid);
    CHECK(BSP_TPS55287_SetVoltageMv(10000U));
    CHECK(BSP_TPS55287_GetStatus().setpoint_valid);
    CHECK(BSP_TPS55287_GetStatus().setpoint_mv == 10000U);
    CHECK(BSP_TPS55287_SetVoltageMv(20000U));
    CHECK(!BSP_TPS55287_SetVoltageMv(20001U));
    CHECK(registers[4] == 0x03U);
    CHECK(BSP_TPS55287_GetVoltageMv(&millivolts));
    CHECK(millivolts == 20000U);
    registers[4] = 0x80U;
    CHECK(BSP_TPS55287_SetInternalFeedback());
    CHECK(!BSP_TPS55287_GetStatus().setpoint_valid);
    CHECK(!BSP_TPS55287_SetEnabled(true));

    if (failures == 0U) puts("TPS55287 tests passed");
    return failures == 0U ? 0 : 1;
}
