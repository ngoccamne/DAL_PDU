#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_eeprom.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M3_Driver/Devices/EEPROM24C04/eeprom24c04.h"

static uint8_t s_memory[EEPROM24C04_SIZE_BYTES];
static bool s_present = true;
static bool s_read_ok = true;
static bool s_write_ok = true;
static uint16_t s_last_address;
static uint16_t s_last_length;
static unsigned int s_failures;
static char s_output[256];

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expression); \
        s_failures++; \
    } \
} while (0)

bool BSP_EEPROM_Probe(void) { return s_present; }

bool BSP_EEPROM_Read(uint16_t address, uint8_t *data, uint16_t length)
{
    s_last_address = address;
    s_last_length = length;
    if (!s_read_ok || address + length > sizeof(s_memory)) return false;
    memcpy(data, &s_memory[address], length);
    return true;
}

bool BSP_EEPROM_Write(uint16_t address, const uint8_t *data, uint16_t length)
{
    s_last_address = address;
    s_last_length = length;
    if (!s_write_ok || address + length > sizeof(s_memory)) return false;
    memcpy(&s_memory[address], data, length);
    return true;
}

void Command_OutputString(CommandContext *context, const char *text)
{
    (void)context;
    if (strlen(s_output) + strlen(text) < sizeof(s_output)) strcat(s_output, text);
    else s_failures++;
}

static int Run(int argc, char *argv[])
{
    s_output[0] = '\0';
    return Cmd_eeprom(NULL, argc, argv);
}

int main(void)
{
    char *probe[] = {"eeprom", "probe"};
    char *read[] = {"eeprom", "read", "0xff", "4"};
    char *read_one[] = {"eeprom", "read", "256"};
    char *write[] = {"eeprom", "write", "0xff", "A1b2c3d4"};
    char *write_last[] = {"eeprom", "write", "511", "0x5a"};
    char *cross_end[] = {"eeprom", "write", "0x1ff", "abcd"};
    char *odd_hex[] = {"eeprom", "write", "0", "abc"};
    char *bad_hex[] = {"eeprom", "write", "0", "gg"};
    char *too_many_bytes[] = {"eeprom", "write", "0", "00112233445566778899aabbccddeeff00"};
    char *bad_address[] = {"eeprom", "read", "0x200"};
    char *bad_count[] = {"eeprom", "read", "0", "17"};
    char *cross_read[] = {"eeprom", "read", "511", "2"};
    char *unknown[] = {"eeprom", "erase"};

    memset(s_memory, 0xFF, sizeof(s_memory));
    CHECK(Run(2, probe) == CMDLINE_OK);
    CHECK(strcmp(s_output, "EEPROM present\r\n") == 0);
    s_present = false;
    CHECK(Run(2, probe) == CMDLINE_OK);
    CHECK(strcmp(s_output, "EEPROM unavailable\r\n") == 0);
    s_present = true;

    CHECK(Run(4, write) == CMDLINE_OK);
    CHECK(strcmp(s_output, "EEPROM write OK\r\n") == 0);
    CHECK(s_last_address == 255U && s_last_length == 4U);
    CHECK(s_memory[255] == 0xA1U && s_memory[256] == 0xB2U);
    CHECK(Run(4, read) == CMDLINE_OK);
    CHECK(strcmp(s_output, "EEPROM 0x00ff: a1 b2 c3 d4\r\n") == 0);
    CHECK(Run(3, read_one) == CMDLINE_OK);
    CHECK(strcmp(s_output, "EEPROM 0x0100: b2\r\n") == 0);
    CHECK(Run(4, write_last) == CMDLINE_OK);
    CHECK(s_memory[511] == 0x5AU);

    CHECK(Run(4, cross_end) == CMDLINE_INVALID_ARG);
    CHECK(Run(4, odd_hex) == CMDLINE_INVALID_ARG);
    CHECK(Run(4, bad_hex) == CMDLINE_INVALID_ARG);
    CHECK(Run(4, too_many_bytes) == CMDLINE_INVALID_ARG);
    CHECK(Run(3, bad_address) == CMDLINE_INVALID_ARG);
    CHECK(Run(4, bad_count) == CMDLINE_INVALID_ARG);
    CHECK(Run(4, cross_read) == CMDLINE_INVALID_ARG);
    CHECK(Run(2, unknown) == CMDLINE_INVALID_ARG);
    CHECK(Run(1, probe) == CMDLINE_TOO_FEW_ARGS);
    CHECK(Run(3, write) == CMDLINE_TOO_FEW_ARGS);
    CHECK(Run(5, write) == CMDLINE_TOO_MANY_ARGS);
    CHECK(s_output[0] == '\0');

    s_read_ok = false;
    CHECK(Run(4, read) == CMDLINE_OK);
    CHECK(strcmp(s_output, "EEPROM read failed\r\n") == 0);
    s_read_ok = true;
    s_write_ok = false;
    CHECK(Run(4, write) == CMDLINE_OK);
    CHECK(strcmp(s_output, "EEPROM write failed\r\n") == 0);

    if (s_failures != 0U)
    {
        fprintf(stderr, "%u EEPROM command checks failed\n", s_failures);
        return 1;
    }
    puts("EEPROM command tests passed");
    return 0;
}
