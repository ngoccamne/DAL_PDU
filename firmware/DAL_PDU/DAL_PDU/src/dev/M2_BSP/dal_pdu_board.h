#ifndef DAL_PDU_BOARD_H_
#define DAL_PDU_BOARD_H_

#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 12000000UL /* 12 MHz crystal, decided 2026-10-06 (BOM-002). */
#endif

/* Heartbeat LED: PA3. */
#define LED_BLINK_DDR    DDRA
#define LED_BLINK_PORT   PORTA
#define LED_BLINK_PIN    PA3

/* USART0, xTP transport: RXD0 = PE0, TXD0 = PE1. */
#define UART_XTP_RX_DDR  DDRE
#define UART_XTP_RX_PORT PORTE
#define UART_XTP_RX_PIN  PE0
#define UART_XTP_TX_DDR  DDRE
#define UART_XTP_TX_PORT PORTE
#define UART_XTP_TX_PIN  PE1
#define UART_XTP_BAUD    115200UL

/* USART1, RS485: RXD1 = PD2, TXD1 = PD3, direction = PD5. */
#define RS485_RX_DDR     DDRD
#define RS485_RX_PORT    PORTD
#define RS485_RX_PIN     PD2
#define RS485_TX_DDR     DDRD
#define RS485_TX_PORT    PORTD
#define RS485_TX_PIN     PD3
#define RS485_DIR_DDR    DDRD
#define RS485_DIR_PORT   PORTD
#define RS485_DIR_PIN    PD5

/* Software UART console: TX = PE3, RX = PE4. */
#define SOFTUART_TX_DDR   DDRE
#define SOFTUART_TX_PORT  PORTE
#define SOFTUART_TX_PIN   PE3
#define SOFTUART_RX_DDR   DDRE
#define SOFTUART_RX_PORT  PORTE
#define SOFTUART_RX_PIN_R PINE
#define SOFTUART_RX_PIN   PE4
#define SOFTUART_BAUD     9600UL

/* External TPL5010 watchdog: WAKE = PB5, DONE = PB6. */
#define EXT_WDT_WAKE_DDR  DDRB
#define EXT_WDT_WAKE_PORT PORTB
#define EXT_WDT_WAKE_PIN  PB5
#define EXT_WDT_FEED_DDR  DDRB
#define EXT_WDT_FEED_PORT PORTB
#define EXT_WDT_FEED_PIN  PB6

/* Hardware TWI: SCL = PD0, SDA = PD1. */
#define TWI_SCL_DDR       DDRD
#define TWI_SCL_PORT      PORTD
#define TWI_SCL_PIN       PD0
#define TWI_SDA_DDR       DDRD
#define TWI_SDA_PORT      PORTD
#define TWI_SDA_PIN       PD1
#define TWI_SCL_FREQ_HZ   100000UL

/* U14 24C04 write protect: PA0 high = protected, low = writable. */
#define EEPROM_WP_DDR     DDRA
#define EEPROM_WP_PORT    PORTA
#define EEPROM_WP_PIN     PA0

/* U10 PAC1934: R50/R51/R53/R55 are 10 milliohm current shunts. */
#define PAC1934_SHUNT_MOHM 10U

/* TRANS eFuse: active-low enable on PC2, active-low fault on PC4 (12V TRANS).
 * 12V_MCU eFuse (U2): active-low fault on PC0. Reported only, it cannot be switched. */
#define TRANS_EN_DDR      DDRC
#define TRANS_EN_PORT     PORTC
#define TRANS_EN_PIN      PC2
#define TRANS_FAULT_PIN_R PINC
#define TRANS_FAULT_A_PIN PC4
#define MCU_FAULT_PIN PC0

/* TC1047A temperature sensor: PF1/ADC1. */
#define TEMP_ADC_CHANNEL  1U

/* SPI CAN controller: SCK = PB1, MOSI = PB2, MISO = PB3. */
#define SPI_DDR           DDRB
#define SPI_PORT          PORTB
#define SPI_SCK_PIN       PB1
#define SPI_MOSI_PIN      PB2
#define SPI_MISO_PIN      PB3
#define MCP2515_CS_DDR    DDRB
#define MCP2515_CS_PORT   PORTB
#define MCP2515_CS_PIN    PB0
#define MCP2515_INT_DDR   DDRB
#define MCP2515_INT_PORT  PORTB
#define MCP2515_INT_PIN_R PINB
#define MCP2515_INT_PIN   PB4

#endif /* DAL_PDU_BOARD_H_ */
