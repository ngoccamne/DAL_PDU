#ifndef APP_CONSOLE_H_
#define APP_CONSOLE_H_

#include <stdbool.h>

/* Initialize software-UART text CLI and hardware UART0 xTP/xCLI. */
bool AppConsole_Init(void);

/* Service the CLI and xTP every 10 ms from main-loop context. */
void AppConsole_Run(void);

#endif /* APP_CONSOLE_H_ */
