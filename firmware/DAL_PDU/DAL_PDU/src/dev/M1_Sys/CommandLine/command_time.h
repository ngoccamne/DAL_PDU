#ifndef COMMAND_TIME_H_
#define COMMAND_TIME_H_

#include <stdint.h>

#include "command.h"

int Cmd_time(CommandContext *context, int argc, char *argv[]);

/* Writes Unix seconds as YYYY-MM-DDTHH:MM:SSZ, or "unset". Shared with the event listing. */
void Command_OutputClock(CommandContext *context, uint32_t unix_s);

/* Writes UNSET, RESTORED, or SET. */
void Command_OutputClockSource(CommandContext *context, uint8_t source);

#endif /* COMMAND_TIME_H_ */
