#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App/app_temperature.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/cmdline.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine/command_temp.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData/system_data.h"
#include "../../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NTC/bsp_ntc.h"

#define TEMP_TEST_CHANNEL 1U

static unsigned failures;
static bool adc_init_called;
static bool adc_fail;
static uint16_t adc_counts;
static char output[128];

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); failures++; \
} } while (0)

void ADC_Init(void)
{
    adc_init_called = true;
}

bool ADC_Read(uint8_t channel, uint16_t *counts)
{
    if (adc_fail || channel != TEMP_TEST_CHANNEL || counts == 0) return false;
    *counts = adc_counts;
    return true;
}

static void capture_write(void *user, const char *text)
{
    (void)user;
    strncat(output, text, sizeof(output) - strlen(output) - 1U);
}

void Command_OutputString(CommandContext *context, const char *text)
{
    if (context != 0 && context->write != 0 && text != 0)
    {
        context->write(context->user, text);
    }
}

static int run_temp(int argc, char *argv[])
{
    CommandContext context = {capture_write, 0, false, 0U};
    output[0] = '\0';
    return Cmd_temp(&context, argc, argv);
}

static void test_conversion(void)
{
    int16_t tenths = 0;
    CHECK(BSP_NTC_ConvertTenthsC(2464U, 16U, &tenths) && tenths == 251);
    CHECK(BSP_NTC_ConvertTenthsC(328U, 16U, &tenths) && tenths == -400);
    CHECK(BSP_NTC_ConvertTenthsC(5734U, 16U, &tenths) && tenths == 1249);
    CHECK(!BSP_NTC_ConvertTenthsC(320U, 16U, &tenths));
    CHECK(!BSP_NTC_ConvertTenthsC(5800U, 16U, &tenths));
    CHECK(!BSP_NTC_ConvertTenthsC(2464U, 0U, &tenths));
    CHECK(!BSP_NTC_ConvertTenthsC(2464U, 16U, 0));
}

static void test_read_and_app(void)
{
    int16_t tenths = 0;
    SystemData_TemperatureStatus status;

    SystemData_Init();
    adc_fail = false;
    adc_counts = 154U;
    CHECK(BSP_NTC_ReadTenthsC(&tenths) && tenths == 251);

    adc_fail = true;
    CHECK(!BSP_NTC_ReadTenthsC(&tenths));
    CHECK(!BSP_NTC_Init());
    adc_fail = false;

    adc_init_called = false;
    CHECK(BSP_NTC_Init() && adc_init_called);

    AppTemperature_Init();
    status = SystemData_GetTemperatureStatus();
    CHECK(status.valid && status.tenths_c == 251);

    adc_fail = true;
    AppTemperature_Init();
    status = SystemData_GetTemperatureStatus();
    CHECK(!status.valid);
    adc_fail = false;

    adc_fail = true;
    AppTemperature_Update();
    status = SystemData_GetTemperatureStatus();
    CHECK(!status.valid && status.tenths_c == 0);
    adc_fail = false;
}

static void test_command(void)
{
    char *none[] = {"temp"};
    char *get[] = {"temp", "get"};
    char *refresh[] = {"temp", "refresh"};
    char *bogus[] = {"temp", "bogus"};
    char *extra[] = {"temp", "get", "x"};

    SystemData_Init();

    adc_fail = false;
    adc_counts = 154U;
    AppTemperature_Update();
    CHECK(run_temp(1, none) == CMDLINE_OK);
    CHECK(strcmp(output, "TEMP 25.1 C\r\n") == 0);

    adc_counts = 102U;
    CHECK(run_temp(2, get) == CMDLINE_OK);
    CHECK(strcmp(output, "TEMP 25.1 C\r\n") == 0);
    CHECK(run_temp(2, refresh) == CMDLINE_OK);
    CHECK(strcmp(output, "TEMP -0.2 C\r\n") == 0);

    adc_counts = 21U;
    CHECK(run_temp(2, refresh) == CMDLINE_OK);
    CHECK(strcmp(output, "TEMP -39.8 C\r\n") == 0);

    CHECK(run_temp(2, bogus) == CMDLINE_INVALID_ARG);
    CHECK(run_temp(3, extra) == CMDLINE_TOO_MANY_ARGS);

    adc_fail = true;
    CHECK(run_temp(2, refresh) == CMDLINE_OK);
    CHECK(strcmp(output, "TEMP unavailable\r\n") == 0);
    adc_fail = false;
}

int main(void)
{
    test_conversion();
    test_read_and_app();
    test_command();
    if (failures != 0U)
    {
        fprintf(stderr, "%u temperature test failure(s)\n", failures);
        return 1;
    }
    printf("temperature tests passed\n");
    return 0;
}
