/*
 * command.h
 *
 */
#ifndef COMMAND_H_
#define COMMAND_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Each command receives the writer for the interface that supplied it. */
typedef struct
{
    void (*write)(void *user, const char *text);
    void *user;
    bool help_pending;
    uint8_t help_index;
} CommandContext;

void Command_OutputString(CommandContext *context, const char *text);
#ifdef __AVR__
#include <avr/pgmspace.h>
/* Writes text stored in flash. */
void Command_OutputFlashString(CommandContext *context, PGM_P text);
#define COMMAND_OUT(context, literal) Command_OutputFlashString((context), PSTR(literal))
#else
#define COMMAND_OUT(context, literal) Command_OutputString((context), (literal))
#endif
/* Sends one help entry per call so a bounded console FIFO can drain. */
void Command_ContinueOutput(CommandContext *context);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_H_ */
