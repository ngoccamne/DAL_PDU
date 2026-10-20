#ifndef TEST_AVR_IO_H_
#define TEST_AVR_IO_H_

#include <stdint.h>

#define _BV(bit) (1U << (bit))

extern volatile uint8_t UCSR0A, UCSR0B, UCSR0C, UBRR0H, UBRR0L, UDR0;
extern volatile uint8_t UCSR1A, UCSR1B, UCSR1C, UBRR1H, UBRR1L, UDR1;
extern volatile uint8_t TCCR1A, TCCR1B, SREG;
extern volatile uint16_t TCNT1, OCR1A;
extern volatile uint8_t TIMSK, TCCR0, OCR0, TCNT0, TCCR2, OCR2, TCNT2, TIFR;
extern volatile uint8_t DDRE, PORTE, PINE;
extern volatile uint8_t DDRA, PORTA;
extern volatile uint8_t DDRB, PORTB, DDRC, PORTC, DDRD, PORTD;
extern volatile uint8_t PINC;
extern volatile uint8_t PINB;
extern volatile uint8_t DDRF, PORTF, DDRG, PORTG;
extern volatile uint8_t MCUCR;
extern volatile uint8_t TWBR, TWSR, TWDR, TWCR;

#define U2X0 1
#define U2X1 1
#define TXC1 6
#define UPE1 2
#define DOR1 3
#define FE1 4
#define TXEN1 3
#define RXEN1 4
#define UDRIE1 5
#define RXCIE1 7
#define TXCIE1 6
#define UCSZ10 1
#define UCSZ11 2
#define UPM10 4
#define UPM11 5
#define USBS1 3
#define WGM12 3
#define CS11 1
#define OCIE1A 4
#define OCF1A 4
#define UPE0 2
#define DOR0 3
#define FE0 4
#define TXEN0 3
#define RXEN0 4
#define UDRIE0 5
#define RXCIE0 7
#define UCSZ00 1
#define UCSZ01 2
#define CS21 1
#define CS20 0
#define WGM21 3
#define OCIE2 7
#define OCF2 7
#define CS00 0
#define CS01 1
#define WGM01 3
#define OCIE0 1
#define OCF0 1
#define PE0 0
#define PE1 1
#define PE3 3
#define PE4 4
#define PA0 0
#define PA3 3
#define PB5 5
#define PB6 6
#define PC0 0
#define PC2 2
#define PC4 4
#define PD0 0
#define PD1 1
#define PD2 2
#define PD3 3
#define PD5 5
#define SRE 7
#define TWEN 2
#define TWSTO 4
#define TWSTA 5
#define TWEA 6
#define TWINT 7

#endif
