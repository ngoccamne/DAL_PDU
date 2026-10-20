#include "command_limit.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmdline.h"
#include "../../M0_App/app_limits.h"

/* Parses an optional minus sign and decimal digits. Returns false on any other text. */
static bool ParseSigned(const char *text, int32_t *result)
{
    bool negative = false;
    int32_t value = 0;
    if (text == 0 || *text == '\0') return false;
    if (*text == '-')
    {
        negative = true;
        text++;
    }
    if (*text == '\0') return false;
    while (*text != '\0')
    {
        if (*text < '0' || *text > '9') return false;
        value = value * 10 + (*text - '0');
        /* Saturate above the int16 range so out-of-range values are refused, not wrapped. */
        if (value > 30000L) value = 30001L;
        text++;
    }
    *result = negative ? -value : value;
    return true;
}

static void OutputNumber(CommandContext *context, int32_t value)
{
    char digits[12];
    uint8_t count = 0U;
    uint32_t magnitude = value < 0 ? (uint32_t)(-value) : (uint32_t)value;
    if (value < 0) COMMAND_OUT(context, "-");
    do
    {
        digits[count++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U && count < sizeof(digits));
    while (count > 0U)
    {
        char out[2] = {digits[--count], '\0'};
        Command_OutputString(context, out);
    }
}

static void OutputLimits(CommandContext *context)
{
    BSP_NVM_Limits limits;
    AppLimits_Get(&limits);
    COMMAND_OUT(context, "LIMIT temp=");
    OutputNumber(context, limits.temp_trip_tenths_c);
    COMMAND_OUT(context, " vout=");
    OutputNumber(context, limits.vout_max_ma);
    COMMAND_OUT(context, " trans=");
    OutputNumber(context, limits.trans5v_max_ma);
    COMMAND_OUT(context, "\r\n");
}

int Cmd_limit(CommandContext *context, int argc, char *argv[])
{
    BSP_NVM_Limits limits;
    int32_t value;

    if (argc > 3) return CMDLINE_TOO_MANY_ARGS;
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "get") == 0))
    {
        OutputLimits(context);
        return CMDLINE_OK;
    }
    if (argc != 3) return CMDLINE_INVALID_ARG;
    if (!ParseSigned(argv[2], &value)) return CMDLINE_INVALID_ARG;

    AppLimits_Get(&limits);
    if (strcmp(argv[1], "temp") == 0)
    {
        limits.temp_trip_tenths_c = (int16_t)value;
    }
    else if (strcmp(argv[1], "vout") == 0)
    {
        limits.vout_max_ma = (uint16_t)value;
    }
    else if (strcmp(argv[1], "trans") == 0)
    {
        limits.trans5v_max_ma = (uint16_t)value;
    }
    else
    {
        return CMDLINE_INVALID_ARG;
    }

    if (!AppLimits_Set(&limits))
    {
        COMMAND_OUT(context, "LIMIT refused: out of range\r\n");
        return CMDLINE_OK;
    }
    OutputLimits(context);
    return CMDLINE_OK;
}
