# Build and package the PDU firmware

Run the root `Makefile` from the PDU repository directory. It reads the checked-in Microchip Studio `.cproj` source list, refreshes the firmware's Git build identity, and compiles for ATmega128A. Install Python 3, GNU Make, and AVR-GCC. The script finds AVR-GCC on `PATH` or in the Microchip Studio 7 toolchain. If installed elsewhere, set `AVR_TOOLCHAIN` to its `bin` directory; set `AVR_DFP` to the ATmega device pack directory if needed. Packaging also needs the sibling AVR bootloader repository at `../Bootloader/AVR_Booyloader/AVRBootLoader`.

## Commands and outputs

```powershell
make original
make signed
make clean
```

| Target | Result |
| --- | --- |
| `make original` | Generates `build_id.h`, then builds `release/DAL_PDU.elf` and the raw `release/DAL_PDU.hex`. |
| `make signed` or `make` | Rebuilds the original image, then creates `release/DAL_PDU_signed.hex` for the AVR bootloader. |
| `make clean` | Removes those three images and `release/DAL_PDU.map`; preserves the build ID counter and other files in `release/`. |

The raw image starts at flash address zero and is limited to addresses below `0x1DF00`. The signed target calls `boot_host.py package`, which checks the raw HEX and adds the 256-byte metadata page at `0x1DF00`. That page records image size, CRC, hardware ID, and numeric build ID. **The `signed` target produces a CRC-checked package, not a cryptographic signature.** Use the packaged HEX with `xbld_client`. See [boot memory layout](boot_memory_layout.md) and [OTA bootloader handoff](ota_bootloader.md).

## Numeric build ID

`signed_build_id.txt` holds the last successful packaged ID. Every `make signed` run increments it, even if the source is unchanged. To make the next package ID `42`, put `41` in the file before running `make signed`. The counter advances only after packaging succeeds. `make original` and `make clean` leave it unchanged. The counter is distinct from the Git hash shown by the firmware's `version` command; see [Versioning](code_implementation/versioning.md).

To use a packager in another location, run `make signed BOOT_HOST=path/to/boot_host.py`. For a Microchip Studio build instead, generate the Git ID with `python tools/gen_build_id.py`, build **Release** in Studio, and use `python tools/release.py Release` to copy its outputs into versioned filenames.
