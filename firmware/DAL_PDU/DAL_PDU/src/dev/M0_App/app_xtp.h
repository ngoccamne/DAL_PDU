#ifndef APP_XTP_H_
#define APP_XTP_H_

/* Service xTP/xCLI on the hardware USART0. The soft UART remains the local CLI. */
void AppXtp_Init(void);
void AppXtp_Run(void);

#endif /* APP_XTP_H_ */
