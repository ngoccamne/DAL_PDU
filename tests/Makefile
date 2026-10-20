CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -O0 -g

SCHEDULER_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/Scheduler
COMMAND_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/CommandLine
SYSTEM_DATA_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M1_Sys/SystemData
FIFO_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M4_MiddleWare/FIFO
DRIVER_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M3_Driver/Component
BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/UART
LED_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/LED
TIMER_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Timer
WATCHDOG_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Watchdog
RESET_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Reset
NVM_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NVM
POWER_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Power
I2C_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/I2C
TPS_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/TPS55287
TPS_DRIVER_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M3_Driver/Devices/TPS55287
PAC_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/PAC1934
PAC_DRIVER_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M3_Driver/Devices/PAC1934
NTC_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/NTC
EEPROM_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/EEPROM
EEPROM_DRIVER_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M3_Driver/Devices/EEPROM24C04
APP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M0_App
MODBUS_INCLUDE_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M4_MiddleWare/ThirdParty/freemodbus/modbus/include
MODBUS_BSP_DIR := ../firmware/DAL_PDU/DAL_PDU/src/dev/M2_BSP/Modbus
UART_INTERFACE_DIR := $(DRIVER_DIR)/UART
SCHEDULER_INCLUDES := -Isupport -I$(SCHEDULER_DIR)
SCHEDULER_SOURCES := unit/test_scheduler.c $(SCHEDULER_DIR)/scheduler.c
COMMAND_SOURCES := unit/test_command_line.c $(COMMAND_DIR)/cmdline.c $(COMMAND_DIR)/command.c \
	$(COMMAND_DIR)/command_led.c $(COMMAND_DIR)/command_trans.c $(COMMAND_DIR)/command_i2c.c \
	$(COMMAND_DIR)/command_persist.c \
	$(SYSTEM_DATA_DIR)/system_data.c
FIFO_SOURCES := unit/test_fifo.c $(FIFO_DIR)/fifo.c
UART_DRIVER_SOURCES := $(FIFO_DIR)/fifo.c \
	$(DRIVER_DIR)/UART0/uart0.c $(DRIVER_DIR)/SoftUART/soft_uart.c \
	$(DRIVER_DIR)/ConsoleUART/console_uart.c $(UART_INTERFACE_DIR)/uart.c \
	$(BSP_DIR)/bsp_uart.c
UART_SOURCES := unit/test_uart_drivers.c $(UART_DRIVER_SOURCES)
APP_SOURCES := unit/test_app_main.c $(APP_DIR)/app_main.c \
	$(APP_DIR)/app_console.c $(APP_DIR)/app_xtp.c $(APP_DIR)/app_heartbeat.c $(APP_DIR)/app_watchdog.c $(APP_DIR)/app_power.c $(APP_DIR)/app_retry.c $(APP_DIR)/app_power_monitor.c \
	$(COMMAND_DIR)/cmdline.c $(COMMAND_DIR)/command.c $(COMMAND_DIR)/command_led.c \
	$(COMMAND_DIR)/command_trans.c $(COMMAND_DIR)/command_i2c.c $(COMMAND_DIR)/command_tps55287.c $(COMMAND_DIR)/command_pac1934.c $(COMMAND_DIR)/command_eeprom.c $(COMMAND_DIR)/command_temp.c $(COMMAND_DIR)/command_telemetry.c $(COMMAND_DIR)/command_setpoint.c $(COMMAND_DIR)/command_bootinfo.c $(COMMAND_DIR)/command_time.c $(COMMAND_DIR)/command_status.c $(COMMAND_DIR)/command_reset.c $(COMMAND_DIR)/command_persist.c $(COMMAND_DIR)/command_fault.c $(COMMAND_DIR)/command_mode.c $(APP_DIR)/app_config.c $(COMMAND_DIR)/command_config.c $(APP_DIR)/app_boot.c $(APP_DIR)/app_watchdog_note.c $(COMMAND_DIR)/command_limit.c $(COMMAND_DIR)/command_event.c $(APP_DIR)/app_setpoint.c $(APP_DIR)/app_mode.c $(APP_DIR)/app_limits.c $(APP_DIR)/app_clock.c support/fake_clock_nvm.c $(APP_DIR)/app_event.c $(SYSTEM_DATA_DIR)/system_data.c \
	$(APP_DIR)/app_temperature.c $(NTC_BSP_DIR)/bsp_ntc.c \
	$(SCHEDULER_DIR)/scheduler.c $(TIMER_BSP_DIR)/bsp_timer.c $(WATCHDOG_BSP_DIR)/bsp_watchdog.c \
	$(LED_BSP_DIR)/bsp_led.c $(POWER_BSP_DIR)/bsp_power.c $(I2C_BSP_DIR)/bsp_i2c.c \
	$(TPS_BSP_DIR)/bsp_tps55287.c $(TPS_DRIVER_DIR)/tps55287.c $(PAC_BSP_DIR)/bsp_pac1934.c $(PAC_DRIVER_DIR)/pac1934.c \
	$(EEPROM_BSP_DIR)/bsp_eeprom.c $(EEPROM_DRIVER_DIR)/eeprom24c04.c $(RESET_BSP_DIR)/bsp_reset.c \
	$(DRIVER_DIR)/DIO/avr/do.c $(UART_DRIVER_SOURCES)
DO_SOURCES := unit/test_do.c $(DRIVER_DIR)/DIO/avr/do.c
WATCHDOG_SOURCES := unit/test_watchdog.c $(APP_DIR)/app_watchdog.c $(APP_DIR)/app_watchdog_note.c \
	$(WATCHDOG_BSP_DIR)/bsp_watchdog.c $(SCHEDULER_DIR)/scheduler.c $(SYSTEM_DATA_DIR)/system_data.c
TARGETS := test_scheduler.exe test_scheduler_semaphore.exe test_command_line.exe test_fifo.exe test_uart_drivers.exe test_do.exe test_watchdog.exe test_app_main.exe test_modbus.exe test_modbus_port.exe test_tps55287.exe test_pac1934.exe test_eeprom24c04.exe test_command_eeprom.exe test_temperature.exe test_fault_latch.exe test_app_mode.exe test_app_limits.exe test_app_retry.exe test_nvm_config.exe test_app_boot.exe test_i2c_bus.exe test_app_config.exe test_command_system.exe test_app_setpoint.exe test_app_clock.exe

.PHONY: all test clean build_id

build_id:
	python ../tools/gen_build_id.py


all: build_id $(TARGETS)

test_scheduler.exe: $(SCHEDULER_SOURCES) support/util/atomic.h
	$(CC) $(CFLAGS) $(SCHEDULER_INCLUDES) $(SCHEDULER_SOURCES) -o $@

test_scheduler_semaphore.exe: $(SCHEDULER_SOURCES) support/util/atomic.h
	$(CC) $(CFLAGS) -DUSE_SCH_SEMAPHORE $(SCHEDULER_INCLUDES) $(SCHEDULER_SOURCES) -o $@

test_command_line.exe: $(COMMAND_SOURCES) $(COMMAND_DIR)/cmdline.h $(COMMAND_DIR)/command.h
	$(CC) $(CFLAGS) -Isupport -I$(COMMAND_DIR) $(COMMAND_SOURCES) -o $@

test_fifo.exe: $(FIFO_SOURCES) $(FIFO_DIR)/fifo.h support/util/atomic.h
	$(CC) $(CFLAGS) -Isupport -I$(FIFO_DIR) $(FIFO_SOURCES) -o $@

test_uart_drivers.exe: $(UART_SOURCES) support/avr/io.h support/avr/interrupt.h support/util/atomic.h
	$(CC) $(CFLAGS) -Isupport -I$(DRIVER_DIR)/UART0 -I$(DRIVER_DIR)/SoftUART \
		-I$(DRIVER_DIR)/ConsoleUART -I$(UART_INTERFACE_DIR) -I$(BSP_DIR) \
		$(UART_SOURCES) -o $@

test_do.exe: $(DO_SOURCES) $(DRIVER_DIR)/DIO/do.h support/avr/io.h support/util/atomic.h
	$(CC) $(CFLAGS) -Isupport $(DO_SOURCES) -o $@

test_watchdog.exe: $(WATCHDOG_SOURCES) support/avr/io.h support/util/atomic.h
	$(CC) $(CFLAGS) -Isupport $(WATCHDOG_SOURCES) -o $@

test_app_main.exe: $(APP_SOURCES) support/avr/io.h support/avr/interrupt.h support/util/atomic.h support/util/delay.h
	$(CC) $(CFLAGS) -Isupport $(APP_SOURCES) -o $@

test_modbus.exe: unit/test_modbus.c $(APP_DIR)/app_modbus.c $(SYSTEM_DATA_DIR)/system_data.c
	$(CC) $(CFLAGS) -Isupport -I$(MODBUS_BSP_DIR) -I$(MODBUS_INCLUDE_DIR) $^ -o $@

test_modbus_port.exe: unit/test_modbus_port.c $(MODBUS_BSP_DIR)/bsp_modbus.c $(DRIVER_DIR)/UART1/uart1.c
	$(CC) $(CFLAGS) -Isupport -I$(MODBUS_BSP_DIR) -I$(MODBUS_INCLUDE_DIR) $^ -o $@

test_tps55287.exe: unit/test_tps55287.c $(COMMAND_DIR)/command_tps55287.c $(TPS_BSP_DIR)/bsp_tps55287.c $(TPS_DRIVER_DIR)/tps55287.c $(SYSTEM_DATA_DIR)/system_data.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_pac1934.exe: unit/test_pac1934.c $(COMMAND_DIR)/command_pac1934.c $(APP_DIR)/app_power_monitor.c $(PAC_BSP_DIR)/bsp_pac1934.c $(PAC_DRIVER_DIR)/pac1934.c $(SYSTEM_DATA_DIR)/system_data.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_eeprom24c04.exe: unit/test_eeprom24c04.c $(EEPROM_BSP_DIR)/bsp_eeprom.c $(EEPROM_DRIVER_DIR)/eeprom24c04.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_command_eeprom.exe: unit/test_command_eeprom.c $(COMMAND_DIR)/command_eeprom.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_temperature.exe: unit/test_temperature.c $(APP_DIR)/app_temperature.c $(NTC_BSP_DIR)/bsp_ntc.c $(COMMAND_DIR)/command_temp.c $(SYSTEM_DATA_DIR)/system_data.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_app_mode.exe: unit/test_app_mode.c $(APP_DIR)/app_mode.c $(SYSTEM_DATA_DIR)/system_data.c $(SCHEDULER_DIR)/scheduler.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_app_limits.exe: unit/test_app_limits.c $(COMMAND_DIR)/command_limit.c $(COMMAND_DIR)/command_fault.c $(COMMAND_DIR)/command_time.c $(COMMAND_DIR)/command_event.c $(APP_DIR)/app_limits.c $(APP_DIR)/app_clock.c support/fake_clock_nvm.c $(APP_DIR)/app_event.c $(APP_DIR)/app_mode.c $(SYSTEM_DATA_DIR)/system_data.c $(SCHEDULER_DIR)/scheduler.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_app_retry.exe: unit/test_app_retry.c $(APP_DIR)/app_retry.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_nvm_config.exe: unit/test_nvm_config.c $(NVM_BSP_DIR)/bsp_nvm.c
	$(CC) $(CFLAGS) -Wno-int-to-pointer-cast -DE2END=4095 -Isupport $^ -o $@

test_app_boot.exe: unit/test_app_boot.c $(APP_DIR)/app_boot.c $(APP_DIR)/app_watchdog_note.c $(COMMAND_DIR)/command_bootinfo.c
	$(CC) $(CFLAGS) -Isupport -I$(COMMAND_DIR) $^ -o $@

test_i2c_bus.exe: unit/test_i2c_bus.c $(I2C_BSP_DIR)/bsp_i2c.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_app_config.exe: unit/test_app_config.c $(APP_DIR)/app_config.c $(COMMAND_DIR)/command_config.c \
	$(COMMAND_DIR)/command_limit.c $(COMMAND_DIR)/command_time.c $(COMMAND_DIR)/command_event.c $(APP_DIR)/app_limits.c $(APP_DIR)/app_clock.c support/fake_clock_nvm.c $(APP_DIR)/app_event.c $(APP_DIR)/app_mode.c \
	$(SYSTEM_DATA_DIR)/system_data.c $(SCHEDULER_DIR)/scheduler.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_command_system.exe: unit/test_command_system.c $(COMMAND_DIR)/command_status.c $(COMMAND_DIR)/command_reset.c $(COMMAND_DIR)/command_persist.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_app_setpoint.exe: unit/test_app_setpoint.c $(APP_DIR)/app_setpoint.c $(APP_DIR)/app_mode.c $(APP_DIR)/app_clock.c support/fake_clock_nvm.c $(APP_DIR)/app_event.c \
	$(SYSTEM_DATA_DIR)/system_data.c $(SCHEDULER_DIR)/scheduler.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_app_clock.exe: unit/test_app_clock.c $(APP_DIR)/app_clock.c support/fake_clock_nvm.c $(COMMAND_DIR)/command_time.c $(APP_DIR)/app_event.c $(APP_DIR)/app_mode.c \
	$(SYSTEM_DATA_DIR)/system_data.c $(SCHEDULER_DIR)/scheduler.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test_fault_latch.exe: unit/test_fault_latch.c $(APP_DIR)/app_power.c $(APP_DIR)/app_mode.c $(SYSTEM_DATA_DIR)/system_data.c $(SCHEDULER_DIR)/scheduler.c
	$(CC) $(CFLAGS) -Isupport $^ -o $@

test: all
	./test_scheduler.exe
	./test_scheduler_semaphore.exe
	./test_command_line.exe
	./test_fifo.exe
	./test_uart_drivers.exe
	./test_do.exe
	./test_watchdog.exe
	./test_app_main.exe
	./test_modbus.exe
	./test_modbus_port.exe
	./test_tps55287.exe
	./test_pac1934.exe
	./test_eeprom24c04.exe
	./test_command_eeprom.exe
	./test_temperature.exe
	./test_fault_latch.exe
	./test_app_limits.exe
	./test_app_retry.exe
	./test_nvm_config.exe
	./test_app_boot.exe
	./test_i2c_bus.exe
	./test_app_mode.exe
	./test_app_config.exe
	./test_command_system.exe
	./test_app_setpoint.exe
	./test_app_clock.exe

clean:
ifeq ($(OS),Windows_NT)
	@cmd /C "if exist test_scheduler.exe del /Q test_scheduler.exe"
	@cmd /C "if exist test_scheduler_semaphore.exe del /Q test_scheduler_semaphore.exe"
	@cmd /C "if exist test_command_line.exe del /Q test_command_line.exe"
	@cmd /C "if exist test_fifo.exe del /Q test_fifo.exe"
	@cmd /C "if exist test_uart_drivers.exe del /Q test_uart_drivers.exe"
	@cmd /C "if exist test_do.exe del /Q test_do.exe"
	@cmd /C "if exist test_watchdog.exe del /Q test_watchdog.exe"
	@cmd /C "if exist test_app_main.exe del /Q test_app_main.exe"
	@cmd /C "if exist test_modbus.exe del /Q test_modbus.exe"
	@cmd /C "if exist test_modbus_port.exe del /Q test_modbus_port.exe"
	@cmd /C "if exist test_tps55287.exe del /Q test_tps55287.exe"
	@cmd /C "if exist test_pac1934.exe del /Q test_pac1934.exe"
	@cmd /C "if exist test_eeprom24c04.exe del /Q test_eeprom24c04.exe"
	@cmd /C "if exist test_command_eeprom.exe del /Q test_command_eeprom.exe"
	@cmd /C "if exist test_app_retry.exe del /Q test_app_retry.exe"
	@cmd /C "if exist test_nvm_config.exe del /Q test_nvm_config.exe"
else
	rm -f $(TARGETS)
endif
