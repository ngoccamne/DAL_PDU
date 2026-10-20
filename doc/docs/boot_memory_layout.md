# AVR bootloader and application memory layout

The PDU uses an ATmega128A with 128 KiB of program flash. Flash addresses below are **byte addresses**; the ATmega128A datasheet gives many vector and boot-section addresses in **words** (one word is two bytes). The application and bootloader occupy fixed, separate regions. There is one application slot.

## Program flash map

```text
Byte address    Contents
0x00000  +----------------------------+  Application reset vector and IRQ table
         | Application code and data  |  Linked from 0x00000
0x1DF00  +----------------------------+  End of linkable application image
         | Image metadata (256 bytes) |  0x1DF00..0x1DFFF
0x1E000  +----------------------------+  Bootloader reset vector and IRQ table
         | Bootloader (8 KiB)         |  Linked from 0x1E000
0x20000  +----------------------------+  End of flash (exclusive)
```

The PDU project sets `__TEXT_REGION_LENGTH__=0x1DF00` in both build configurations, leaving the metadata page outside the linked application. The AVR packager adds that page to the application HEX; it contains the image identity, size, CRC, and entry information used for validation. The bootloader linker uses `__TEXT_REGION_ORIGIN__=0x1E000` and `__TEXT_REGION_LENGTH__=0x2000`. The maximum linked application region is therefore `0x00000..0x1DEFF` (122,624 bytes). EEPROM, including the boot request and update journal, has a separate address space.

## Reset and interrupt vectors

Both images have their own vector table at the start of their linked region:

| Image | Reset vector | First interrupt vector slot |
| --- | ---: | ---: |
| Application | `0x00000` (word `0x0000`) | `0x00004` (word `0x0002`) |
| Bootloader | `0x1E000` (word `0xF000`) | `0x1E004` (word `0xF002`) |

Program the **BOOTRST fuse** and select the **4096-word boot section** (`BOOTSZ1:0 = 00`). On hardware reset, BOOTRST sends execution to the bootloader reset vector at `0x1E000`. The interrupt vector location is controlled separately by `IVSEL`: `IVSEL=0` selects the application table; `IVSEL=1` selects the boot table. The current bootloader keeps interrupts disabled, so it does not rely on its interrupt table. Before entering the application it clears `IVSEL`, allowing application interrupts to use the table at zero. See the [ATmega128A datasheet, Interrupt Vectors and Boot Size Configuration](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-8151-8-bit-AVR-ATmega128A_Datasheet.pdf).

## How control reaches the application

On startup, the bootloader consumes any EEPROM boot request. Without a request, it checks that the update journal marks the image valid and verifies the packaged image CRC before its automatic jump. An explicit update request keeps it in bootloader mode. The host's `bjump` command can also request a jump after verification.

Before jumping, `avr_jump_application()` disables interrupts and the watchdog, stops USART0 and Timer1, masks and clears timer interrupts, clears `IVSEL`, and resets `RAMPZ`. It then clears AVR register `r1` and executes `jmp 0`. Address zero is the **application reset vector**, not `main()`. The application's C startup code initializes the stack and runtime data before calling `main()`; the application enables interrupts later in its own initialization. This is a software jump, so it is not a full hardware reset of every peripheral.

For packaging, upload, and the application's watchdog reset into bootloader mode, see [OTA bootloader handoff](ota_bootloader.md).
