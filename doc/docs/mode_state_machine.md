# Mode state machine

This page describes the operating mode as implemented in `M0_App/app_mode.c` (2026-10-05). The mode is
one of four: **NORMAL**, **MANUAL**, **WARNING**, and **ERROR**. The earlier four-state model (Standby,
Running, ProtectedShutdown, SafeFault) is gone.

## 1. Modes

| Mode | Value | Entered when | Outputs (VOUT, TRANS) | Commands | Regulator monitored |
|---|---:|---|---|---|---|
| **NORMAL** | 0 | No error, and no manual request | Allowed. Limits act. | All | Yes |
| **MANUAL** | 1 | Requested by command, from NORMAL, WARNING, or ERROR (revised 2026-10-15: outranks ERROR once requested) | Allowed, including `trans on`/`tps55287 on`, regardless of a regulator/limit cause underneath. Limits log only. | All | No |
| **WARNING** | 2 | A confirmed eFuse FLT (PC4 or PC0), unreadable PAC1934, I2C timeout, setpoint mismatch, or Modbus link loss | Outputs remain commanded as before. Limits log only. | All | Yes |
| **ERROR** | 3 | The regulator (U5) fails when the TRANS eFuse FLT is inactive, reports a protection fault, or stops responding; or a limit trips in NORMAL | Both off, unless MANUAL has since been requested | All | No |

The mode is **derived** on each call to `AppMode_Get()`. The first matching rule wins:

1. MANUAL, if a manual request is active (revised 2026-10-15: checked before ERROR, so a manual
   request always wins — see OI-34).
2. ERROR, if the regulator error flag or limit error flag is set.
3. WARNING, if an eFuse fault is latched, the PAC1934 is unreadable, an I2C timeout is active,
   the setpoint readback mismatches, or Modbus link loss is active.
4. NORMAL otherwise.

Every mode accepts all commands and responds. Regulator and limit ERROR causes and the eFuse WARNING
latch are independent. A healthy `mode normal` or `fault clear` request clears regulator/limit causes;
`fault clear` also removes an inactive eFuse latch. The latch does not block leaving MANUAL. The regulator is not watched
automatically in MANUAL or ERROR.
With automatic retry enabled (default), the 100 ms retry task waits the configured interval
(default 5 s) after a regulator or limit ERROR. It probes the regulator, checks live limits,
then powers TRANS on and restores VOUT only if it was enabled before shutdown. The PAC1934 must
have a valid sample, and an active PC4 fault prevents a successful restore. A failed attempt
leaves ERROR set and waits another interval. The previous VOUT request is held in RAM; after a
reset, recovery can enable TRANS but cannot restore that request. MANUAL suspends automatic attempts. `mode retry`
and Modbus `40030` = 4 request an immediate attempt.
`fault clear all` additionally refreshes the PAC1934, EEPROM, I²C, and setpoint diagnostics and
restarts the Modbus link timeout. Any condition still present remains reported.

## 2. Transitions

| From | To | Trigger | Actions |
|---|---|---|---|
| TRANS enable attempt | ERROR | Regulator not detected once TRANS is powered | Both outputs off; TRANS is turned back off |
| Startup | WARNING | PAC1934 not detected | None |
| Startup | ERROR | Limit ERROR flag stored from an earlier trip | Both outputs off until a healthy retry or clear request |
| Startup | NORMAL | No error | None |
| NORMAL or WARNING | ERROR | Regulator status fault (SCP, OCP, or OVP), or regulator stops responding | Both outputs off |
| NORMAL | ERROR | Over-temperature or overcurrent limit exceeded for the required samples (`PROT-021`, `PROT-022`) | Both outputs off. WARNING event logged. |
| MANUAL, WARNING, or ERROR | (no change) | Over-temperature or overcurrent limit exceeded | WARNING event logged. Outputs unchanged. |
| NORMAL or WARNING | WARNING | PC4 (while TRANS is on) or PC0 reads continuously active for 50 ms | Latch in EEPROM; no software output shutdown |
| NORMAL or WARNING | WARNING | PAC1934 unreadable | None |
| NORMAL or WARNING | WARNING | No valid Modbus request for the configured timeout, after at least one was seen (`SRS-COM-045`) | None. Outputs are not turned off by this. |
| WARNING | NORMAL | Cause clears (PAC1934 readable, or a new valid Modbus request arrives) | None |
| NORMAL, WARNING, or ERROR | MANUAL | `mode manual`, or Modbus holding register `40030` = 1 (revised 2026-10-15: reachable from ERROR too) | None. The regulator/limit/eFuse-latch cause, if any, is untouched. |
| ERROR | NORMAL or WARNING | Automatic retry, `mode retry`, or Modbus holding register `40030` = 4 | Checks regulator and live limits; if healthy, clears ERROR, enables TRANS, and restores a VOUT request saved before shutdown. An eFuse warning latch remains. |
| MANUAL or ERROR | NORMAL or WARNING | `mode normal`, or Modbus holding register `40030` = 0 | Checks regulator and limits; clears those causes only when healthy. An eFuse latch does not block the request. |
| WARNING or ERROR | NORMAL or WARNING, unless MANUAL was requested | `fault clear`, refused while an eFuse pin or limit is active, or the regulator is unavailable | Clears the eFuse latch and healthy regulator/limit ERROR causes. Outputs stay as commanded. |

A confirmed eFuse fault raises WARNING and remains latched until `fault clear`.

Requests to enter NORMAL while a regulator/limit health check fails are refused. The console prints `MODE refused: error`, and Modbus returns an
exception. Requests to enter MANUAL
are never refused for this reason (revised 2026-10-15, OI-34).

## 3. Regulator monitor

The one-second rail monitor (`AppPowerMonitor_Update`) reads the TPS55287 status register while the mode is
NORMAL or WARNING **and TRANS is on**. The TPS55287 (U5) sits behind the TRANS eFuse and is unpowered whenever
TRANS is off, so the monitor does not poll it then — polling an unpowered chip would otherwise read back as a
failure and set ERROR for no reason. A live PC4 eFuse FLT also suspends the regulator read because
hardware may have removed TPS power. Otherwise an SCP, OCP, or OVP flag, or a failed read while TRANS
is on, sets the ERROR flag. The flags are cleared by the read. ERROR is
cleared by a healthy normal-mode request, fault clear, or retry attempt.

The same reasoning applies to enabling TRANS: `AppPower_SetTrans(true)` and `AppPower_EnsureRegulatorOn()`
(used by `tps55287 set`/`on` and the equivalent Modbus writes) wait 20 ms after powering TRANS on,
then probe the TPS55287. If it does not answer while PC4 FLT is inactive, they set ERROR and turn TRANS
back off. An active PC4 FLT leaves TRANS commanded on. After a successful probe, they also
verify register `0x04` is `0x03` (internal feedback, 20 V scale), correcting it if a raw I2C
command changed it. A failed readback sets ERROR in the same way as a failed presence probe.
Setpoints use 800–20000 mV in 10 mV steps.

`mode normal` probes the regulator through `AppPower_ProbeRegulator()` when a regulator or limit ERROR is set, so it also works from a
cold boot with TRANS off: it powers TRANS on just long enough to check, then restores TRANS to whatever it was before, whatever the
result. That lets `mode normal` clear a persisted **limit** ERROR too, not only a regulator ERROR —
`AppMode_Retry` requires both causes to be clear before it drops either one, so without this probe a stale
limit ERROR left over from before a reset could never be cleared while TRANS was off.

## 4. Outputs

- **ERROR**: `AppPower_Update` turns VOUT off, then TRANS off, on every 6 ms pass while TRANS is on — unless
  MANUAL has been requested since, in which case outputs stay under manual control. TRANS cannot be enabled
  from ERROR itself (only from MANUAL, which outranks it).
- **Link timeout** (`SRS-COM-045`, revised 2026-10-15): if no Modbus request has arrived for the configured
  timeout (default 5 s) after the first one, this raises the `link` WARNING cause in `mode get`. It no longer
  turns VOUT or TRANS off by itself, and is not persisted (plain RAM state, cleared the instant a new valid
  Modbus request arrives). Previously this turned outputs off without changing the mode; that was reversed
  because in practice it was tripping during console-only bench use with no Modbus traffic at all, not an
  actual lost link in the field — see OI-03.
- **Latched eFuse fault** (`SRS-PROT-020`): PC4 (while TRANS is on) or PC0 must read continuously
  active for 50 ms before it is treated as real; an inactive sample resets the timer. Once confirmed,
  it is stored in EEPROM and raises WARNING in NORMAL mode. It does not trigger a software output
  shutdown or block a later TRANS enable. `fault get` and `mode get` report the latch until `fault clear`.
  If PC4 is active and the TPS55287 cannot answer because its supply has tripped, software keeps the
  TRANS enable pin asserted. The eFuse's hardware protection may still interrupt power.
- **Over-temperature and overcurrent** (`PROT-021`, `PROT-022`): checked every second in every mode. Temperature
  needs two consecutive samples at or above the limit. Each current needs three consecutive samples above it.
  - Every trip is logged once as a WARNING event in the 24C04, whatever the mode. A trip is logged again only
    after the value has returned under the limit.
  - In NORMAL, the trip also turns VOUT and then TRANS off and sets the limit ERROR.
  - In MANUAL, WARNING, and ERROR, the trip only logs. Outputs are not changed.
- **Limit ERROR end**: an automatic or manual retry, `mode normal`, or `fault clear` clears it once
  the regulator responds and every limit is back in range. The flag is stored in internal EEPROM,
  so the ERROR survives a reset until a healthy recovery succeeds.
  The health check does not leave an output on.

## 5. Self-test

The startup self-test runs once in `APP_Initialize`, before the scheduler starts. The TPS55287 (U5) sits
behind the TRANS eFuse, which stays off at boot by design, so it cannot be probed here. It is not part of
the startup self-test: it is checked on demand instead, the first time TRANS is enabled (see
[section 3](#3-regulator-monitor)).

| Device | Result | Effect |
|---|---|---|
| U10 PAC1934 | Missing | WARNING |
| U14 24C04 | Missing | Recorded only. Shown by `mode` as `eeprom=1`. |

The self-test is synchronous, so it is never visible as a mode.

## 6. LED indication

The heartbeat LED (PA3) shows the mode. Patterns are in `app_heartbeat.c`.

| Mode | Pattern | ON / OFF (ms) |
|---|---|---:|
| NORMAL | Slow heartbeat | 1000 / 1000 (configurable with the `led` command and Modbus `40020`–`40023`) |
| MANUAL | Medium blink | 200 / 800 |
| WARNING | Double flash | 100 / 100, then 100 / 1000 |
| ERROR | Rapid blink | 50 / 50 |

The heartbeat enable (Modbus coil `00010`, `led` command) applies to the NORMAL pattern only. Faults always show.

## 7. Console and Modbus

| Interface | Operation |
|---|---|
| `mode` or `mode get` | Prints the mode, then `CAUSES regulator=0 pac1934=0 eeprom=0 latch=0 limit=0 setpoint=0 bus=0 link=0`. A 1 means that cause is active. `regulator` and `limit` are ERROR causes; `latch`, `pac1934`, `setpoint`, `bus`, and `link` are WARNING causes (`eeprom` only records itself, per section 5). |
| `mode normal` | Checks the regulator and limits when an ERROR cause is set, clears healthy causes, then requests NORMAL. An eFuse latch remains a WARNING. |
| `mode manual` | Requests MANUAL. Never refused for being in ERROR (revised 2026-10-15, OI-34) — the regulator/limit/latch cause, if any, is untouched underneath. |
| `limit`, `limit temp <tenths C>`, `limit vout <mA>`, `limit trans <mA>` | Shows or sets the limits. Stored in internal EEPROM. |
| `mode retry` | Immediately tries recovery from a regulator or limit ERROR and restores the prior output request if healthy. Does not leave MANUAL. |
| `config`, `config set address`, `config set baud`, `config set timeout`, `config set retry_enable`, `config set retry_time`, `config_reset confirm` | Shows or sets the Modbus and retry settings, or restores all settings to the defaults. See [Command reference](command_reference.md#configuration). |
| `event get` | Lists the events in the 24C04 ring, with the clock time. |
| Modbus input `30022` | Mode value: 0 NORMAL, 1 MANUAL, 2 WARNING, 3 ERROR. |
| Modbus holding `40030` | Read: mode value. Write 0 requests NORMAL, 1 requests MANUAL, and 4 requests an immediate retry. |
| Modbus holding `40034` and `40035` | Persistent automatic retry enable (0 or 1, default 1) and interval (1 to 60 s, default 5). |
| Modbus holding `40031`–`40033` | Limits: temperature in tenths of a degree C (signed), VOUT in mA, +5_TRANS in mA. A refused write returns an exception. |

## 8. Known limits

- The limit defaults are 85.0 °C for two samples, and 1000 mA for three samples on VOUT and on +5_TRANS. They
  need bench confirmation. The values are in the ATmega128A internal EEPROM, as `SRS-CFG-060` now requires (OI-11).
- A limit ERROR is kept across a reset in internal EEPROM. It clears on a healthy automatic or manual retry,
  `fault clear`, or `mode normal` request, once the limits are back in range. A reboot loses the saved
  VOUT request, so a retry after boot enables TRANS only.
- The Modbus limit registers `40031`–`40033` are in the implemented map. The design draft has no equivalent (OI-14).
- The startup self-test checks that the PAC1934 and 24C04 acknowledge. It does not check their configuration,
  and it no longer includes the TPS55287 (see [section 5](#5-self-test)).
- The regulator presence check runs on demand when TRANS is enabled, rather than at startup.
  The enable path verifies internal feedback mode before enabling VOUT.
- The link timeout raises the `link` WARNING cause and no longer turns outputs off by itself (revised
  2026-10-15; see the open question below, now resolved).
- The eFuse fault debounce is fixed at 50 ms (`EFUSE_FAULT_DEBOUNCE_MS` in `app_power.c`), not yet bench-tuned
  against the actual inrush duration on this board.
- Only the regulator fault flags are monitored. Other regulator faults (for example thermal) are not yet read.
- An I2C timeout from the TPS55287 sets ERROR while the regulator is monitored (NORMAL or WARNING) and TRANS is
  on, as the regulator rule requires (OI-26). In MANUAL it sets only the WARNING cause (`bus=1` in `mode`), and
  the mode stays MANUAL. A timeout from any other device sets WARNING until a transaction completes. A NACK from
  an absent device is not a fault.
- The bootloader is not implemented, so there is no bootloader mode.

## 9. Open question

- **Link timeout (Q7 follow-up) — resolved 2026-10-15.** Decided the other way from the original 2026-10-05
  call: link loss now sets WARNING and no longer turns outputs off by itself. See section 4 and OI-03.
