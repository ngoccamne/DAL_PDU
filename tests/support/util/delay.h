#ifndef TEST_UTIL_DELAY_H_
#define TEST_UTIL_DELAY_H_

/* APP_Main's endless blink loop is not run by host tests. */
static inline void _delay_ms(double milliseconds)
{
    (void)milliseconds;
}

#endif /* TEST_UTIL_DELAY_H_ */
