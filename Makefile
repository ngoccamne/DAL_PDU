PYTHON ?= python
BOOT_HOST ?= ../Bootloader/AVR_Booyloader/AVRBootLoader/tools/boot_host.py

.PHONY: all version original signed clean

all: signed

version:
	$(PYTHON) tools/gen_build_id.py

original: version
	$(PYTHON) tools/build_firmware.py

signed: original
	$(PYTHON) tools/package_signed.py --boot-host "$(BOOT_HOST)"

clean:
	$(PYTHON) tools/build_firmware.py --clean
