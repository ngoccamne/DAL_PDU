#ifndef TEST_AVR_INTERRUPT_H_
#define TEST_AVR_INTERRUPT_H_

#define ISR(vector, ...) void vector(void)
#define ISR_NOBLOCK
#define sei() ((void)0)
#define cli() ((void)0)

#endif
