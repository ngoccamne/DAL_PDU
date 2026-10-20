#include "command_i2c.h"
#include "command.h"
#include "cmdline.h"
#include "../../M2_BSP/I2C/bsp_i2c.h"

#include <stdbool.h>
#include <stdint.h>

static bool ParseNumber(const char *text, uint8_t maximum, uint8_t *value)
{
    uint16_t parsed = 0U;
    uint8_t base = 10U;
    if (text == 0 || value == 0 || *text == '\0') return false;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        text += 2;
        base = 16U;
        if (*text == '\0') return false;
    }
    while (*text != '\0')
    {
        uint8_t digit;
        char c = *text++;
        if (c >= '0' && c <= '9') digit = (uint8_t)(c - '0');
        else if (base == 16U && c >= 'a' && c <= 'f') digit = (uint8_t)(c - 'a' + 10);
        else if (base == 16U && c >= 'A' && c <= 'F') digit = (uint8_t)(c - 'A' + 10);
        else return false;
        if (digit >= base || parsed > (uint16_t)((maximum - digit) / base)) return false;
        parsed = (uint16_t)(parsed * base + digit);
    }
    *value = (uint8_t)parsed;
    return true;
}

static void OutputHex(CommandContext *context, uint8_t value)
{
    static const char digits[] = "0123456789abcdef";
    char text[5] = {'0', 'x', digits[value >> 4], digits[value & 15U], '\0'};
    Command_OutputString(context, text);
}

int Cmd_i2cdetect(CommandContext *context, int argc, char *argv[])
{
    uint8_t first = 0x08U;
    uint8_t last = 0x77U;
    uint8_t address;
    if (argc != 1 && argc != 3) return argc < 3 ? CMDLINE_TOO_FEW_ARGS : CMDLINE_TOO_MANY_ARGS;
    if (argc == 3 && (!ParseNumber(argv[1], 0x77U, &first) ||
                      !ParseNumber(argv[2], 0x77U, &last) ||
                      first < 0x08U || first > last)) return CMDLINE_INVALID_ARG;
    for (address = first; address <= last; address++)
    {
        if (BSP_I2C_Probe(address))
        {
            OutputHex(context, address);
            COMMAND_OUT(context, " ");
        }
    }
    COMMAND_OUT(context, "\r\n");
    return CMDLINE_OK;
}

int Cmd_i2cget(CommandContext *context, int argc, char *argv[])
{
    uint8_t address;
    uint8_t reg;
    uint8_t value;
    bool ok;
    if (argc < 2) return CMDLINE_TOO_FEW_ARGS;
    if (argc > 3) return CMDLINE_TOO_MANY_ARGS;
    if (!ParseNumber(argv[1], 0x77U, &address) || address < 0x08U) return CMDLINE_INVALID_ARG;
    if (argc == 3)
    {
        if (!ParseNumber(argv[2], 0xFFU, &reg)) return CMDLINE_INVALID_ARG;
        ok = BSP_I2C_ReadRegister(address, reg, &value);
    }
    else ok = BSP_I2C_ReadByte(address, &value);
    if (!ok)
    {
        COMMAND_OUT(context, "I2C error\r\n");
        return CMDLINE_OK;
    }
    OutputHex(context, value);
    COMMAND_OUT(context, "\r\n");
    return CMDLINE_OK;
}

int Cmd_i2cset(CommandContext *context, int argc, char *argv[])
{
    uint8_t address;
    uint8_t reg;
    uint8_t value;
    if (argc < 4) return CMDLINE_TOO_FEW_ARGS;
    if (argc > 4) return CMDLINE_TOO_MANY_ARGS;
    if (!ParseNumber(argv[1], 0x77U, &address) || address < 0x08U ||
        !ParseNumber(argv[2], 0xFFU, &reg) || !ParseNumber(argv[3], 0xFFU, &value))
        return CMDLINE_INVALID_ARG;
    if (!BSP_I2C_WriteRegister(address, reg, value))
    {
        COMMAND_OUT(context, "I2C error\r\n");
        return CMDLINE_OK;
    }
    COMMAND_OUT(context, "I2C write OK\r\n");
    return CMDLINE_OK;
}
