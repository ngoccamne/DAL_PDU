#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM/bsp_nvm.h"

uint8_t test_eeprom[4096];
static unsigned failures;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

int main(void)
{
    BSP_NVM_Config config;
    uint8_t i;
    uint8_t checksum = 0U;
    memset(test_eeprom, 0xFF, sizeof(test_eeprom));
    CHECK(!BSP_NVM_ReadConfig(&config));

    /* Existing version 1 record at address 100 keeps its three settings. */
    test_eeprom[100] = 0xC3U;
    test_eeprom[101] = 1U;
    test_eeprom[102] = 7U;
    test_eeprom[103] = 4U;
    test_eeprom[104] = 12U;
    for (i = 101U; i <= 104U; i++) checksum = (uint8_t)(checksum + test_eeprom[i]);
    test_eeprom[105] = checksum;
    CHECK(BSP_NVM_ReadConfig(&config));
    CHECK(config.slave_address == 7U && config.baud_code == 4U && config.comms_timeout_s == 12U);
    CHECK(config.retry_enable == 1U && config.retry_time_s == 5U);

    config.retry_enable = 0U;
    config.retry_time_s = 9U;
    BSP_NVM_WriteConfig(&config);
    CHECK(test_eeprom[101] == BSP_NVM_CONFIG_VERSION);
    memset(&config, 0, sizeof(config));
    CHECK(BSP_NVM_ReadConfig(&config));
    CHECK(config.slave_address == 7U && config.retry_enable == 0U && config.retry_time_s == 9U);
    test_eeprom[107] ^= 1U;
    CHECK(!BSP_NVM_ReadConfig(&config));

    if (failures != 0U) return 1;
    puts("NVM config tests passed");
    return 0;
}
