#include "command_eeprom.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "../../M2_BSP/EEPROM/bsp_eeprom.h"
#include "../../M3_Driver/Devices/EEPROM24C04/eeprom24c04.h"

#define OUT(context, literal) COMMAND_OUT(context, literal)

static int HexDigit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool ParseNumber(const char *text, uint16_t maximum, uint16_t *result)
{
    uint16_t value = 0U;
    uint8_t base = 10U;
    if (text == 0 || result == 0 || *text == '\0') return false;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        text += 2;
        base = 16U;
        if (*text == '\0') return false;
    }
    while (*text != '\0')
    {
        int digit = HexDigit(*text++);
        if (digit < 0 || digit >= base ||
            value > (uint16_t)((maximum - (uint16_t)digit) / base)) return false;
        value = (uint16_t)(value * base + (uint16_t)digit);
    }
    *result = value;
    return true;
}

static bool ParseHexBytes(const char *text, uint8_t *data, uint8_t *length)
{
    uint8_t count = 0U;
    if (text == 0 || data == 0 || length == 0) return false;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    if (*text == '\0') return false;
    while (*text != '\0')
    {
        int high = HexDigit(*text++);
        int low;
        if (*text == '\0' || count == EEPROM24C04_PAGE_SIZE) return false;
        low = HexDigit(*text++);
        if (high < 0 || low < 0) return false;
        data[count++] = (uint8_t)((high << 4) | low);
    }
    *length = count;
    return true;
}

static void OutputHexByte(CommandContext *context, uint8_t value)
{
    static const char digits[] = "0123456789abcdef";
    char text[3] = {digits[value >> 4], digits[value & 15U], '\0'};
    Command_OutputString(context, text);
}

static void OutputAddress(CommandContext *context, uint16_t address)
{
    OUT(context, "EEPROM 0x");
    OutputHexByte(context, (uint8_t)(address >> 8));
    OutputHexByte(context, (uint8_t)address);
    OUT(context, ":");
}

int Cmd_eeprom(CommandContext *context, int argc, char *argv[])
{
    uint16_t address;
    uint16_t count;
    uint8_t data[EEPROM24C04_PAGE_SIZE];
    uint8_t index;

    if (argc < 2) return CMDLINE_TOO_FEW_ARGS;
    if (strcmp(argv[1], "probe") == 0)
    {
        if (argc > 2) return CMDLINE_TOO_MANY_ARGS;
        if (BSP_EEPROM_Probe()) OUT(context, "EEPROM present\r\n");
        else OUT(context, "EEPROM unavailable\r\n");
        return CMDLINE_OK;
    }
    if (strcmp(argv[1], "read") == 0)
    {
        if (argc < 3) return CMDLINE_TOO_FEW_ARGS;
        if (argc > 4) return CMDLINE_TOO_MANY_ARGS;
        if (!ParseNumber(argv[2], EEPROM24C04_SIZE_BYTES - 1U, &address))
            return CMDLINE_INVALID_ARG;
        count = 1U;
        if (argc == 4 && (!ParseNumber(argv[3], EEPROM24C04_PAGE_SIZE, &count) ||
                          count == 0U)) return CMDLINE_INVALID_ARG;
        if (count > EEPROM24C04_SIZE_BYTES - address) return CMDLINE_INVALID_ARG;
        if (!BSP_EEPROM_Read(address, data, count))
        {
            OUT(context, "EEPROM read failed\r\n");
            return CMDLINE_OK;
        }
        OutputAddress(context, address);
        for (index = 0U; index < count; index++)
        {
            OUT(context, " ");
            OutputHexByte(context, data[index]);
        }
        OUT(context, "\r\n");
        return CMDLINE_OK;
    }
    if (strcmp(argv[1], "write") == 0)
    {
        if (argc < 4) return CMDLINE_TOO_FEW_ARGS;
        if (argc > 4) return CMDLINE_TOO_MANY_ARGS;
        if (!ParseNumber(argv[2], EEPROM24C04_SIZE_BYTES - 1U, &address) ||
            !ParseHexBytes(argv[3], data, &index) ||
            index > EEPROM24C04_SIZE_BYTES - address) return CMDLINE_INVALID_ARG;
        if (BSP_EEPROM_Write(address, data, index)) OUT(context, "EEPROM write OK\r\n");
        else OUT(context, "EEPROM write failed\r\n");
        return CMDLINE_OK;
    }
    return CMDLINE_INVALID_ARG;
}
