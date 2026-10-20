#ifndef APP_MODE_H_
#define APP_MODE_H_

#include <stdbool.h>
#include <stdint.h>

/* MANUAL wins, then ERROR, then WARNING, then NORMAL (revised 2026-10-15: MANUAL used to
 * rank below ERROR; see OI-34). */
typedef enum
{
    APP_MODE_NORMAL = 0,
    APP_MODE_MANUAL = 1,
    APP_MODE_WARNING = 2,
    APP_MODE_ERROR = 3
} AppMode;

/* Outputs turn off after this long without a Modbus request, once one has been seen. */
#define APP_MODE_COMMS_TIMEOUT_MS 5000UL

/* Sets the Modbus silence limit for AppMode_LinkLost. Called at boot and on a config change. */
void AppMode_SetLinkTimeoutMs(uint32_t timeout_ms);

/* Records the startup self-test. A missing regulator is ERROR, but the TPS55287 is no
 * longer checked at startup (OI-31), so the real caller always passes true for it. An
 * unreadable PAC1934 is a WARNING cause. A missing EEPROM is recorded but does not
 * change the mode. */
void AppMode_SelfTest(bool tps55287_ok, bool pac1934_ok, bool eeprom_ok);

AppMode AppMode_Get(void);

/* NORMAL and WARNING watch the regulator. */
bool AppMode_RegulatorMonitored(void);

/* RequestNormal is refused while an ERROR cause remains; its command path checks
 * regulator/limits and clears healthy causes first. RequestManual is allowed from ERROR. */
bool AppMode_RequestManual(void);
bool AppMode_RequestNormal(void);

/* Each latches ERROR until the health check on a normal-mode request succeeds. */
void AppMode_SetRegulatorFault(void);
void AppMode_SetLimitError(void);
/* I2C timeout on any device. WARNING while set. */
void AppMode_SetBusFault(bool fault);
bool AppMode_BusFault(void);
void AppMode_SetPacUnreadable(bool unreadable);
/* Updates the EEPROM diagnostic after an explicit probe. */
void AppMode_SetEepromMissing(bool missing);
/* Setpoint readback mismatch (SRS-PWR-014). WARNING while set. */
void AppMode_SetSetpointMismatch(bool mismatch);
bool AppMode_SetpointMismatch(void);

/* Internal health check used by mode normal. Clears regulator/limit ERROR causes only
 * when the regulator responds and no limit is exceeded. */
bool AppMode_Retry(bool regulator_ok, bool limits_ok);

bool AppMode_RegulatorError(void);
bool AppMode_LimitError(void);
bool AppMode_PacUnreadable(void);
bool AppMode_EepromMissing(void);

/* Called for every valid Modbus request. */
void AppMode_NoteRequest(void);
/* Acknowledges link loss and restarts its timeout without faking a Modbus request. */
void AppMode_RestartLinkTimeout(void);

/* True when a Modbus master has been seen and has since gone silent. */
bool AppMode_LinkLost(void);

#endif /* APP_MODE_H_ */
