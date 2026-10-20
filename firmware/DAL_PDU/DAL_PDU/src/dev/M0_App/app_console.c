#include "app_console.h"
#include "app_watchdog.h"
#include "app_xtp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../M1_Sys/CommandLine/cmdline.h"
#include "../M1_Sys/CommandLine/command.h"
#include "../M2_BSP/UART/bsp_uart.h"
#include "../M4_MiddleWare/FIFO/fifo.h"

#define APP_CONSOLE_COUNT 1U
#define APP_CONSOLE_LINE_CAPACITY 64U
#define APP_CONSOLE_ECHO_CAPACITY 64U
#define APP_CONSOLE_OUTPUT_CAPACITY 256U
#define APP_CONSOLE_RX_BUDGET 16U

typedef struct
{
    uart_number_t port;
    char line[APP_CONSOLE_LINE_CAPACITY];
    uint8_t line_length;
    bool line_overflow;
    bool last_was_cr;
    bool command_pending;
    uint8_t echo_storage[APP_CONSOLE_ECHO_CAPACITY];
    FIFO_t echo_fifo;
    bool echo_pending;
    uint8_t pending_echo_byte;
    CommandContext command;
} AppConsole_Session;

static AppConsole_Session s_sessions[APP_CONSOLE_COUNT] =
{
    {.port = UART_NUMBER_CONSOLE}
};

/* A single response spool has one owner until all bytes are handed to its
 * UART TX FIFO. Help entries stream through it to limit ATmega128A SRAM use. */
static uint8_t s_output_storage[APP_CONSOLE_OUTPUT_CAPACITY];
static FIFO_t s_output_fifo;
static AppConsole_Session *s_output_owner;
static bool s_output_pending;
static uint8_t s_pending_output_byte;
static bool s_output_overflow;
static uint8_t s_next_session;

static void AppConsole_FlushEcho(AppConsole_Session *session)
{
    uint8_t byte;
    if (session->echo_pending)
    {
        if (!BSP_UART_Write(session->port, session->pending_echo_byte)) return;
        session->echo_pending = false;
    }
    while (FIFO_Pop(&session->echo_fifo, &byte))
    {
        if (!BSP_UART_Write(session->port, byte))
        {
            session->pending_echo_byte = byte;
            session->echo_pending = true;
            return;
        }
    }
}

static void AppConsole_QueueEcho(AppConsole_Session *session, const char *text)
{
    while (*text != '\0')
    {
        if (!FIFO_Push(&session->echo_fifo, (uint8_t)*text))
        {
            AppConsole_FlushEcho(session);
            if (!FIFO_Push(&session->echo_fifo, (uint8_t)*text)) return;
        }
        text++;
    }
}

static void AppConsole_WriteResponse(void *user, const char *text)
{
    if (user != s_output_owner || text == NULL) return;
    while (*text != '\0')
    {
        if (!FIFO_Push(&s_output_fifo, (uint8_t)*text))
        {
            s_output_overflow = true;
            return;
        }
        text++;
    }
}

/* The printf middleware has no destination parameter. Only commands run
 * while a response owner is set, so their printf output follows that owner. */
void _putchar(char character)
{
    char text[2] = {character, '\0'};
    if (s_output_owner != NULL) AppConsole_WriteResponse(s_output_owner, text);
}

static void AppConsole_FlushOutput(void)
{
    uint8_t byte;
    if (s_output_owner == NULL) return;
    if (s_output_pending)
    {
        if (!BSP_UART_Write(s_output_owner->port, s_pending_output_byte)) return;
        s_output_pending = false;
    }
    while (FIFO_Pop(&s_output_fifo, &byte))
    {
        if (!BSP_UART_Write(s_output_owner->port, byte))
        {
            s_pending_output_byte = byte;
            s_output_pending = true;
            return;
        }
    }
    if (s_output_owner->command.help_pending)
    {
        Command_ContinueOutput(&s_output_owner->command);
        return;
    }
    /* These bytes follow the response in the same UART TX FIFO. */
    if (s_output_overflow)
    {
        AppConsole_QueueEcho(s_output_owner, "[output truncated]\r\n");
    }
    AppConsole_QueueEcho(s_output_owner, "> ");
    s_output_owner = NULL;
    s_output_overflow = false;
}

bool AppConsole_Init(void)
{
    uint8_t i;
    s_output_owner = NULL;
    s_output_pending = false;
    s_output_overflow = false;
    s_next_session = 0U;
    AppXtp_Init();
    if (!FIFO_Init(&s_output_fifo, s_output_storage, sizeof(s_output_storage)))
        return false;

    for (i = 0U; i < APP_CONSOLE_COUNT; i++)
    {
        AppConsole_Session *session = &s_sessions[i];
        if (!FIFO_Init(&session->echo_fifo, session->echo_storage,
                       sizeof(session->echo_storage))) return false;
        session->line_length = 0U;
        session->line_overflow = false;
        session->last_was_cr = false;
        session->command_pending = false;
        session->echo_pending = false;
        session->command.write = AppConsole_WriteResponse;
        session->command.user = session;
        session->command.help_pending = false;
        session->command.help_index = 0U;
        AppConsole_QueueEcho(session, "> ");
    }
    return AppWatchdog_RegisterClient(APP_WATCHDOG_CLIENT_CONSOLE, 200U);
}

static void AppConsole_ReadSession(AppConsole_Session *session)
{
    uint8_t byte;
    uint8_t budget = APP_CONSOLE_RX_BUDGET;
    if (session->command_pending || s_output_owner == session) return;

    while (budget-- != 0U)
    {
        /* A backspace echoes three bytes; leave room before consuming RX. */
        if (FIFO_Count(&session->echo_fifo) > APP_CONSOLE_ECHO_CAPACITY - 3U ||
            !BSP_UART_Read(session->port, &byte)) break;
        if (byte == '\n' && session->last_was_cr)
        {
            session->last_was_cr = false;
            continue;
        }
        session->last_was_cr = (byte == '\r');
        if (byte == '\r' || byte == '\n')
        {
            AppConsole_QueueEcho(session, "\r\n");
            session->command_pending = true;
            break;
        }
        if (byte == '\b' || byte == 0x7FU)
        {
            if (session->line_length > 0U && !session->line_overflow)
            {
                session->line_length--;
                AppConsole_QueueEcho(session, "\b \b");
            }
        }
        else if (byte >= 0x20U && byte <= 0x7EU && !session->line_overflow)
        {
            if (session->line_length < APP_CONSOLE_LINE_CAPACITY - 1U)
            {
                char echo[2] = {(char)byte, '\0'};
                session->line[session->line_length++] = (char)byte;
                AppConsole_QueueEcho(session, echo);
            }
            else session->line_overflow = true;
        }
    }
}

static void AppConsole_Execute(AppConsole_Session *session)
{
    uint8_t result;
    s_output_owner = session;
    s_output_overflow = false;

    if (session->line_overflow)
        COMMAND_OUT(&session->command, "Line too long\r\n");
    else if (session->line_length > 0U)
    {
        session->line[session->line_length] = '\0';
        result = CmdLineProcess(&session->command, session->line);
        switch (result)
        {
            case CMDLINE_OK: break;
            case CMDLINE_BAD_CMD:
                COMMAND_OUT(&session->command, "Unknown command\r\n");
                break;
            case CMDLINE_TOO_MANY_ARGS:
                COMMAND_OUT(&session->command, "Too many arguments\r\n");
                break;
            case CMDLINE_TOO_FEW_ARGS:
                COMMAND_OUT(&session->command, "Too few arguments\r\n");
                break;
            default:
                COMMAND_OUT(&session->command, "Invalid argument\r\n");
                break;
        }
    }
    session->line_length = 0U;
    session->line_overflow = false;
    session->command_pending = false;
}

void AppConsole_Run(void)
{
    uint8_t i;
    AppXtp_Run();
    for (i = 0U; i < APP_CONSOLE_COUNT; i++) AppConsole_FlushEcho(&s_sessions[i]);
    AppConsole_FlushOutput();
    for (i = 0U; i < APP_CONSOLE_COUNT; i++) AppConsole_ReadSession(&s_sessions[i]);
    for (i = 0U; i < APP_CONSOLE_COUNT; i++) AppConsole_FlushEcho(&s_sessions[i]);

    if (s_output_owner == NULL)
    {
        for (i = 0U; i < APP_CONSOLE_COUNT; i++)
        {
            uint8_t index = (uint8_t)((s_next_session + i) % APP_CONSOLE_COUNT);
            if (s_sessions[index].command_pending)
            {
                AppConsole_Execute(&s_sessions[index]);
                s_next_session = (uint8_t)((index + 1U) % APP_CONSOLE_COUNT);
                break;
            }
        }
    }
    AppConsole_FlushOutput();
    for (i = 0U; i < APP_CONSOLE_COUNT; i++) AppConsole_FlushEcho(&s_sessions[i]);
    AppWatchdog_CheckIn(APP_WATCHDOG_CLIENT_CONSOLE);
}
