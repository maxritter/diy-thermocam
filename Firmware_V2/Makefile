# Reproducible build for DIY-Thermocam V1 (Teensy 3.1/3.2) and V2 (Teensy 3.6) Firmware 2.50
ROOT := $(CURDIR)
BUILD_V1 := $(ROOT)/build/v1
BUILD_V2 := $(ROOT)/build/v2
DIST := $(ROOT)/dist

INC_FLAGS := \
	-I$(ROOT)/src/general \
	-I$(ROOT)/src/gui \
	-I$(ROOT)/src/hardware \
	-I$(ROOT)/src/hardware/camera \
	-I$(ROOT)/src/hardware/display \
	-I$(ROOT)/src/hardware/touchscreen \
	-I$(ROOT)/src/thermal \
	-I$(ROOT)/src/libraries/ADC \
	-I$(ROOT)/src/libraries/Bounce \
	-I$(ROOT)/src/libraries/EEPROM \
	-I$(ROOT)/src/libraries/I2C \
	-I$(ROOT)/src/libraries/Metro \
	-I$(ROOT)/src/libraries/SPI \
	-I$(ROOT)/src/libraries/SdFat \
	-I$(ROOT)/src/libraries/Time \
	-I$(ROOT)/src/libraries/tjpgd

BASE_COMMON := -g -Wall -ffunction-sections -fdata-sections -nostdlib -mno-unaligned-access
NM := $(shell ls -1 $$HOME/Library/Arduino15/packages/teensy/tools/teensy-compile/*/arm/bin/arm-none-eabi-nm 2>/dev/null | head -n 1)

.PHONY: all v1 v2 test release clean

all: test v1 v2

release: all
	cp $(ROOT)/Changelog.txt $(DIST)/Changelog.txt
	cd $(DIST) && rm -f DIYThermocam_2.50.zip && zip -q DIYThermocam_2.50.zip Firmware_V1.hex Firmware_V2.hex Changelog.txt
	@ls -lh $(DIST)/Firmware_V1.hex $(DIST)/Firmware_V2.hex $(DIST)/Changelog.txt $(DIST)/DIYThermocam_2.50.zip

v1:
	@mkdir -p $(BUILD_V1) $(DIST)
	arduino-cli compile \
		--fqbn teensy:avr:teensy31:usb=serial,speed=96,opt=osstd \
		--build-property "build.flags.common=$(BASE_COMMON) $(INC_FLAGS)" \
		--output-dir $(BUILD_V1) \
		.
	@echo "=== Verifying unmangled C startup_late_hook symbol in V1 ELF ==="
	$(NM) $(BUILD_V1)/Firmware_V2.ino.elf | grep -E " T startup_late_hook$$"
	python3 tools/merge_hex.py $(BUILD_V1)/Firmware_V2.ino.hex tools/MassStorage.hex $(DIST)/Firmware_V1.hex

v2:
	@mkdir -p $(BUILD_V2) $(DIST)
	arduino-cli compile \
		--fqbn teensy:avr:teensy36:usb=serial,speed=144,opt=o2std \
		--build-property "build.flags.common=$(BASE_COMMON) $(INC_FLAGS)" \
		--output-dir $(BUILD_V2) \
		.
	cp $(BUILD_V2)/Firmware_V2.ino.hex $(DIST)/Firmware_V2.hex
	@echo "Built V2 HEX -> $(DIST)/Firmware_V2.hex"

test:
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror \
		-I$(ROOT)/src/general \
		$(ROOT)/tests_host/test_v2_core.cpp \
		$(ROOT)/src/general/v2_core.cpp \
		-o $(ROOT)/tests_host/test_v2_core -lm
	$(ROOT)/tests_host/test_v2_core
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror \
		-I$(ROOT)/src/general \
		$(ROOT)/tests_host/test_tc_proto_v2.cpp \
		$(ROOT)/src/general/tc_proto_v2.cpp \
		$(ROOT)/src/general/v2_core.cpp \
		-o $(ROOT)/tests_host/test_tc_proto_v2 -lm
	$(ROOT)/tests_host/test_tc_proto_v2

clean:
	rm -rf $(BUILD_V1) $(BUILD_V2) $(DIST) $(ROOT)/tests_host/test_v2_core $(ROOT)/tests_host/test_tc_proto_v2
