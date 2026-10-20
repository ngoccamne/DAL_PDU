#include "app_config.h"

static const uint32_t s_baud_rates[APP_CONFIG_BAUD_COUNT] = {
    1200UL, 2400UL, 4800UL, 9600UL, 19200UL, 38400UL, 115200UL
};

static BSP_NVM_Config s_config;
static bool s_defaults_in_use;

static void Defaults(BSP_NVM_Config *config)
{
    config->slave_address = APP_CONFIG_SLAVE_ADDRESS_DEFAULT;
    config->baud_code = APP_CONFIG_BAUD_CODE_DEFAULT;
    config->comms_timeout_s = APP_CONFIG_COMMS_TIMEOUT_DEFAULT_S;
    config->retry_enable = APP_CONFIG_RETRY_ENABLE_DEFAULT;
    config->retry_time_s = APP_CONFIG_RETRY_TIME_DEFAULT_S;
}

static bool Valid(const BSP_NVM_Config *config)
{
    return config->slave_address >= APP_CONFIG_SLAVE_ADDRESS_MIN &&
           config->slave_address <= APP_CONFIG_SLAVE_ADDRESS_MAX &&
           config->baud_code < APP_CONFIG_BAUD_COUNT &&
           config->comms_timeout_s >= APP_CONFIG_COMMS_TIMEOUT_MIN_S &&
           config->comms_timeout_s <= APP_CONFIG_COMMS_TIMEOUT_MAX_S &&
           config->retry_enable <= 1U &&
           config->retry_time_s >= APP_CONFIG_RETRY_TIME_MIN_S &&
           config->retry_time_s <= APP_CONFIG_RETRY_TIME_MAX_S;
}

void AppConfig_Init(void)
{
    if (BSP_NVM_ReadConfig(&s_config) && Valid(&s_config))
    {
        s_defaults_in_use = false;
        return;
    }
    Defaults(&s_config);
    s_defaults_in_use = true;
}

void AppConfig_Get(BSP_NVM_Config *config)
{
    if (config != 0) *config = s_config;
}

bool AppConfig_Set(const BSP_NVM_Config *config)
{
    if (config == 0 || !Valid(config)) return false;
    s_config = *config;
    BSP_NVM_WriteConfig(&s_config);
    s_defaults_in_use = false;
    return true;
}

void AppConfig_Reset(void)
{
    Defaults(&s_config);
    BSP_NVM_WriteConfig(&s_config);
    s_defaults_in_use = false;
}

bool AppConfig_DefaultsInUse(void)
{
    return s_defaults_in_use;
}

uint32_t AppConfig_BaudRate(uint8_t baud_code)
{
    return (baud_code < APP_CONFIG_BAUD_COUNT) ? s_baud_rates[baud_code] : 0UL;
}

bool AppConfig_BaudCodeFor(uint32_t baud, uint8_t *baud_code)
{
    uint8_t code;
    if (baud_code == 0) return false;
    for (code = 0U; code < APP_CONFIG_BAUD_COUNT; code++)
    {
        if (s_baud_rates[code] == baud)
        {
            *baud_code = code;
            return true;
        }
    }
    return false;
}
