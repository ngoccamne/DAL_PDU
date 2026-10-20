# Versioning

The firmware's console and Modbus interface report two identities. The bootloader
package also has a separate numeric build ID:

| Identity | Set by | Shows | Changes |
|---|---|---|---|
| **Version** `MAJOR.MINOR.PATCH` and a release stage | A person, in `version.h` | `version` command, Modbus `40102`–`40103`, optional Studio release copy names | Only when someone edits the file, in a reviewed commit |
| **Build** (short git commit, `-dirty` flag) | The build tool, from git | `version` command, Modbus `40107`–`40108`, optional Studio release copy names | Automatically, with each commit and with uncommitted edits |
| **Package build ID** (integer) | `make signed`, from `signed_build_id.txt` | AVR image metadata | On every successful signed build; the file can be edited manually |

There is no build date. A date changes on every rebuild, so two identical sources would produce different images.

## Where the code lives

| File | Role | Edited by |
|---|---|---|
| `firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Version/version.h` | The version numbers and the release stage. It includes `build_id.h`. | A person |
| `firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Version/build_id.h` | The build identity. Generated, and tracked by git. It changes with each build. | `tools/gen_build_id.py` |
| `tools/gen_build_id.py` | Reads git and writes `build_id.h`. It rewrites the file only when its content changes, so an unchanged tree does not recompile. | Nobody, normally |
| `tools/release.py` | Copies the Studio output to `release/` with the version and build in the name. | Nobody, normally |
| `firmware/.../M1_Sys/CommandLine/command.c` | The `version` command. | — |
| `firmware/.../M0_App/app_modbus.c` | Modbus `40102`–`40103` and `40107`–`40108`. | — |

The version is not in `command.h` any more. Only `version.h` holds the version numbers.

## What the version means

- **MAJOR:** an incompatible change for the OBC. A register changes its meaning, or a command changes its reply format. Increase MAJOR and reset MINOR and PATCH to 0.
- **MINOR:** new registers, commands, or events that old clients can ignore. Reset PATCH to 0.
- **PATCH:** a fix with no change to the interface.
- **Release stage:** `"Development"` until the first bench-verified build, then `"Release"`.

Changes to the persistent EEPROM layouts are a separate axis. The event ring, the configuration record, and the clock checkpoint each have their own version byte or magic. Record a layout change in the changelog, and bump the matching byte, so old data is ignored instead of misread.

## Change the version by hand

1. Edit `version.h`: the three `FIRMWARE_VERSION_*` values and, if needed, `FIRMWARE_STATUS`.
2. Update the requirements and the Modbus page if the interface changed.
3. Commit the change, with the new version in the message.
4. Tag the commit when it is released to the bench, for example `fw-v0.2.0`.

The version is not derived from git tags. A build without tags would otherwise report the wrong version.

## Generate the build identity

Run the script from the PDU folder:

```
python tools/gen_build_id.py
```

Output, when the tree has uncommitted changes:

```
build_id.h written: c7f0bf8-dirty
```

Output, when rebuilt with no changes since the last run:

```
build_id.h unchanged: c7f0bf8-dirty
```

The generated header looks like this:

```c
#define FIRMWARE_BUILD_ID "c7f0bf8-dirty"
/* Bit 28 is the dirty flag. Bits 0 to 27 are the short commit hash. */
#define FIRMWARE_BUILD_CODE 0x1C7F0BF8UL
```

- A clean tree gives the plain short hash, for example `c7f0bf8`.
- Uncommitted changes add `-dirty`.
- Without git the build is `nogit`, with code `0x10000000`.

The script tries `git` on the PATH first, then `C:\Program Files\Git\cmd\git.exe` and `...\mingw64\bin\git.exe`.

The host test Makefile runs the script before it compiles anything, so `make -C tests test` always has a current header.
The root `make original` and `make signed` targets run it before compiling; see
[Firmware build and packaging](../firmware_build.md). The package build ID is
independent of this Git identity.

## Build a release copy

Build the firmware in Studio first. Then run:

```
python tools/release.py Debug
```

or `python tools/release.py Release` for the Release configuration. The script copies the image into `release/`:

```
release: release/DAL_PDU_v0.1.0_c7f0bf8-dirty.hex
release: release/DAL_PDU_v0.1.0_c7f0bf8-dirty.elf
```

The script reads the version from `version.h` and the build from `build_id.h`, so run `gen_build_id.py` first, or let the Studio pre-build event do it. It copies the image that is already in the configuration folder, so a stale image gives a release with the new name and the old code.

## Run both steps from Atmel Studio

Open the project properties, then Build Events, and set:

- **Pre-build:** `python "$(ProjectDir)..\..\..\tools\gen_build_id.py"`
- **Post-build:** `python "$(ProjectDir)..\..\..\tools\release.py" $(ConfigurationName)`

Studio then generates the build identity before each compile, and copies a release after each build. `$(ProjectDir)` points at `firmware\DAL_PDU\DAL_PDU\`, so three levels up is the PDU folder.

Without the pre-build step the compile fails, because `build_id.h` is missing from a fresh clone.

## Where the version appears

| Output | Shows |
|---|---|
| Console `version` | `DAL_PDU firmware v0.1.0 (Development) build c7f0bf8-dirty` |
| Modbus `40102` | Major in the high byte, minor in the low byte |
| Modbus `40103` | Patch |
| Modbus `40107`–`40108` | The build code: high word first |
| Release file name | `DAL_PDU_v0.1.0_c7f0bf8-dirty.hex` |

## Notes

- `release/` is in `.gitignore`. `build_id.h` is tracked, so a fresh clone builds once the script has run. Commit it with the release.
- Python must be on the PATH for Studio and for the test Makefile.
- A `-dirty` build is not reproducible from the commit alone. Commit before a release build.
- The current AVR bootloader package has one application slot and a metadata page
  containing CRC and numeric build ID. The numeric ID is not the semantic
  `MAJOR.MINOR.PATCH` version and is not a cryptographic signature.
