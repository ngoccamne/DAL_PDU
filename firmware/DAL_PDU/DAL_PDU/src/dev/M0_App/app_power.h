#ifndef APP_POWER_H_
#define APP_POWER_H_

#include <stdbool.h>
#include <stdint.h>

bool AppPower_Init(void);
void AppPower_Update(void);
/* Rejects enable in ERROR (unless MANUAL); an eFuse fault is WARNING only. */
bool AppPower_SetTrans(bool enable);
/* No-op if TRANS is already on. Otherwise same as AppPower_SetTrans(true): powers TRANS
 * and confirms the TPS55287 answers before any command that needs it. */
bool AppPower_EnsureRegulatorOn(void);
/* For `mode normal`: probes the regulator even while in ERROR (which AppPower_SetTrans
 * would otherwise refuse) and restores TRANS to how it was found. Does not change the
 * mode by itself. */
bool AppPower_ProbeRegulator(void);
/* Turns VOUT off, then TRANS off. Does not latch anything. */
void AppPower_TurnOffOutputs(void);
/* Clears the latched eFuse faults in internal EEPROM. Refused while a fault input is
 * active. Does not enable TRANS. */
bool AppPower_ClearFaultLatch(void);

#endif /* APP_POWER_H_ */
