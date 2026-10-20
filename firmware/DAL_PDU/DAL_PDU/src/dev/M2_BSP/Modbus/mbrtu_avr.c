/* Compile upstream RTU code with its unused STM32 CRC temporary tolerated
 * on AVR, keeping the FreeMODBUS submodule at its recorded commit. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "../../M4_MiddleWare/ThirdParty/freemodbus/modbus/rtu/mbrtu.c"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
