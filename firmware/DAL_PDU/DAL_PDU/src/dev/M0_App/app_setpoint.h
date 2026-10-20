#ifndef APP_SETPOINT_H_
#define APP_SETPOINT_H_

#include <stdint.h>

/* Setpoint readback (SRS-PWR-014). Checked once per second by the rail monitor, only while
 * TRANS and VOUT are on and a setpoint is commanded. The monitor samples once a second, so the
 * settling time is a count of skipped samples: the first two checks after a change are not
 * judged. */
#define APP_SETPOINT_SETTLE_MS 2000UL

/* A sample matches when |measured - setpoint| is within 5 % of the setpoint, and at least
 * 100 mV. */
#define APP_SETPOINT_TOLERANCE_DIVISOR 20U
#define APP_SETPOINT_TOLERANCE_FLOOR_MV 100U

/* Consecutive judged samples outside the tolerance before the mismatch is flagged. */
#define APP_SETPOINT_MISMATCH_SAMPLES 3U

/* Called once per second from the rail monitor, after the PAC1934 sample. A flag is a WARNING
 * cause and logs one WARNING event, with the measured VOUT in mV as the value. */
void AppSetpoint_Update(void);

#endif /* APP_SETPOINT_H_ */
