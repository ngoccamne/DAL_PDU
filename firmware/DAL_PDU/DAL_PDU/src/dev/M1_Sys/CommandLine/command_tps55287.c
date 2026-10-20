#include "command_tps55287.h"
#include "cmdline.h"
#include "../../M0_App/app_power.h"
#include "../../M2_BSP/TPS55287/bsp_tps55287.h"

#include <stdint.h>
#include <string.h>

#ifdef __AVR__
#include <avr/pgmspace.h>
static void OutputFlashString(CommandContext *context, PGM_P source)
{
    char chunk[16];
    uint8_t count = 0U;
    char value;
    do
    {
        value = (char)pgm_read_byte(source++);
        if (value != '\0') chunk[count++] = value;
        if (value == '\0' || count == sizeof(chunk) - 1U)
        {
            chunk[count] = '\0';
            Command_OutputString(context, chunk);
            count = 0U;
        }
    } while (value != '\0');
}
#define OUT(context, literal) OutputFlashString(context, PSTR(literal))
#else
#define OUT(context, literal) Command_OutputString(context, literal)
#endif

static bool ParseMillivolts(const char *text, uint16_t *value)
{
    uint32_t result = 0U;
    if (text == 0 || *text == '\0') return false;
    while (*text != '\0')
    {
        if (*text < '0' || *text > '9') return false;
        result = result * 10U + (uint8_t)(*text++ - '0');
        if (result > 20000U) return false;
    }
    *value = (uint16_t)result;
    return true;
}

static void OutputNumber(CommandContext *context, uint16_t value)
{
    char text[6];
    uint8_t length = 0U;
    uint8_t index;
    do
    {
        text[length++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    for (index = 0U; index < length / 2U; index++)
    {
        char swap = text[index];
        text[index] = text[length - index - 1U];
        text[length - index - 1U] = swap;
    }
    text[length] = '\0';
    Command_OutputString(context, text);
}

int Cmd_tps55287(CommandContext *context, int argc, char *argv[])
{
    TPS55287_State state;
    uint16_t millivolts;
    if (argc < 2) return CMDLINE_TOO_FEW_ARGS;
    if (argc > 3) return CMDLINE_TOO_MANY_ARGS;

    if (strcmp(argv[1], "set") == 0)
    {
        if (argc < 3) return CMDLINE_TOO_FEW_ARGS;
        if (!ParseMillivolts(argv[2], &millivolts) || millivolts < 800U)
            return CMDLINE_INVALID_ARG;
        if (!AppPower_EnsureRegulatorOn())
            OUT(context, "TPS55287 set failed (TRANS)\r\n");
        else if (!BSP_TPS55287_SetVoltageMv(millivolts))
            OUT(context, "TPS55287 set failed (I2C or range)\r\n");
        else OUT(context, "TPS55287 set OK\r\n");
        return CMDLINE_OK;
    }
    if (argc > 2) return CMDLINE_TOO_MANY_ARGS;

    if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "off") == 0)
    {
        bool enabled = strcmp(argv[1], "on") == 0;
        /* "off" never needs TRANS powered just to turn itself off. */
        if (enabled && !AppPower_EnsureRegulatorOn())
            OUT(context, "TPS55287 enable failed (TRANS)\r\n");
        else if (!BSP_TPS55287_SetEnabled(enabled))
            OUT(context, "TPS55287 enable failed (setpoint, I2C or TRANS)\r\n");
        else if (enabled) OUT(context, "TPS55287 on\r\n");
        else OUT(context, "TPS55287 off\r\n");
        return CMDLINE_OK;
    }
    if (strcmp(argv[1], "get") == 0)
    {
        if (!BSP_TPS55287_GetVoltageMv(&millivolts))
            OUT(context, "TPS55287 read failed (I2C)\r\n");
        else
        {
            OUT(context, "VOUT setpoint ");
            OutputNumber(context, millivolts);
            OUT(context, " mV\r\n");
        }
        return CMDLINE_OK;
    }
    if (strcmp(argv[1], "status") == 0)
    {
        if (!BSP_TPS55287_ReadState(&state))
        {
            OUT(context, "TPS55287 read failed (I2C)\r\n");
            return CMDLINE_OK;
        }
        OUT(context, "TPS55287 addr=0x7");
        if (BSP_TPS55287_Address() == TPS55287_ADDR_MODE_HIGH) OUT(context, "4");
        else OUT(context, "5");
        if (state.mode & 0x80U) OUT(context, " on");
        else OUT(context, " off");
        if (state.feedback & 0x80U) OUT(context, " external-feedback");
        else OUT(context, " internal-feedback");
        if (state.status & 0x80U) OUT(context, " SCP");
        if (state.status & 0x40U) OUT(context, " OCP");
        if (state.status & 0x20U) OUT(context, " OVP");
        OUT(context, "\r\n");
        return CMDLINE_OK;
    }
    return CMDLINE_INVALID_ARG;
}
