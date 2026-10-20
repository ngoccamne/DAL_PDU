#ifndef COMMAND_I2C_H_
#define COMMAND_I2C_H_
#include "command.h"
int Cmd_i2cdetect(CommandContext *context, int argc, char *argv[]);
int Cmd_i2cget(CommandContext *context, int argc, char *argv[]);
int Cmd_i2cset(CommandContext *context, int argc, char *argv[]);
#endif
