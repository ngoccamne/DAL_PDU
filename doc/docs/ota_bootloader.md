# OTA bootloader handoff

The PDU application uses USART0 (PE0/PE1) at 115200 baud for the AVR bootloader's
XTP/XCLI transport. The local text console remains on PE3/PE4. UART0 emits no
startup text or prompt. The application accepts the `0xA2` XTP profile: CRC16/XMODEM,
ECC, seven-bit ARQ sequence, and no addressing or segmentation. It acknowledges
valid XCLI requests on ID `0x0100` and responds on ID `0x0101`.

While the application runs, `xbld_client` can send XCLI `ping 0` (command ID `0x02`)
and `reboot 0` (ID `0x04`). Ping reports `mode=app`. Reboot turns VOUT and TRANS
off, replies, then writes the boot request at internal EEPROM `0xFC0..0xFC1` and
resets by watchdog. Application EEPROM erase preserves the bootloader's
`0xF80..0xFC1` journal and request region. The bootloader consumes the request,
holds for updates, and handles erase/write/verify/jump using its own commands.

An OBC can request the same bootloader entry through Modbus holding register `40060`:
write `0xB007` using FC 06 or a one-register FC 16 request. The application sends
the normal Modbus acknowledgment, waits 100 ms for it to transmit, turns VOUT and
TRANS off, then sets the boot request and resets. Reading `40060` returns 1 while
the request is pending and 0 otherwise. Other write values are rejected. The
firmware image itself is still transferred over UART0 XTP, not Modbus.

The SAMV71 application and both bootloaders use the same XTP/XCLI framing for this
reset exchange. Their image layouts and update commands differ. Use the AVR
`host-client-port` build for this board; the AVR bootloader has one application
slot and its metadata page begins at flash address `0x1DF00`.
See [AVR bootloader and application memory layout](boot_memory_layout.md) for the
flash map, vector tables, and jump sequence.

## Build and package the PDU application

From the PDU repository root, run:

```powershell
make signed
```

This refreshes the Git build identity, compiles the original image at
`release/DAL_PDU.hex`, increments `signed_build_id.txt`, and adds the 256-byte
AVR metadata page to `release/DAL_PDU_signed.hex`. The metadata contains a CRC;
it is not a cryptographic signature. See [Build and package the PDU firmware](firmware_build.md)
for prerequisites, counter adjustment, and cleanup. Upload the **packaged** HEX:

```powershell
../Bootloader/AVR_Booyloader/host-client-port/build-windows/xbld_client.exe --port COM5 --baud 115200 --flash release/DAL_PDU_signed.hex
```

`--flash` itself sends the application reboot request, then erases, writes,
verifies, and jumps. Replace `COM5` for the board. The client uses one baud rate
for both the application and bootloader. A board with an existing 9600-baud
bootloader must have that bootloader reflashed at 115200 before this OTA path works.

## Bootloader installation

Build the AVR bootloader for the board's **12 MHz** crystal and 115200 baud:

```powershell
python AVRBootLoader/tools/build.py --f-cpu 12000000 --baud 115200 --external-watchdog
```

The bootloader's Timer1 conversion now accepts 12 MHz; its build script still
defaults to 16 MHz, so pass `--f-cpu` explicitly. The external watchdog option
feeds the PDU's TPL5010 during long transfers. Flash the bootloader with BOOTRST
and the 8 KiB boot section selected, and confirm the application image ends before
`0x1DF00`. Verify ping, reboot, erase, transfer, verify, and jump on the
assembled board.
