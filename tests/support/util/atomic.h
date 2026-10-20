#ifndef TEST_UTIL_ATOMIC_H_
#define TEST_UTIL_ATOMIC_H_

/* Host tests are single-threaded. This shim preserves block syntax only;
 * it does not verify interrupt exclusion on the AVR target. */
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(type) for (int atomic_once = 1; atomic_once; atomic_once = 0)

#endif
