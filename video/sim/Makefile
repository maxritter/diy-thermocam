# Film frames from the real Firmware 4.0 UI, built on the host simulator in firmware/sim.
FW := ../../firmware
CXX := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wno-write-strings -Wno-unused-parameter -Wno-deprecated-declarations \
	-Wno-ignored-qualifiers -Wno-vla-cxx-extension -Ishims -I$(FW)/sim/shims -I$(FW)/include

SRCS := film.cpp \
	$(FW)/sim/sim_display.cpp \
	$(FW)/sim/sim_stubs.cpp \
	$(FW)/src/hardware/display/fonts.cpp \
	$(FW)/src/general/colorschemes.cpp \
	$(FW)/src/core/tc_thermal_core.cpp \
	$(FW)/src/gui/gui.cpp \
	$(FW)/src/gui/buttons.cpp \
	$(FW)/src/gui/livemode.cpp \
	$(FW)/src/gui/mainmenu.cpp \
	$(FW)/src/gui/templimitsmenu.cpp \
	$(FW)/src/gui/settingsmenu.cpp \
	$(FW)/src/gui/videomenu.cpp \
	$(FW)/src/gui/loadmenu.cpp \
	$(FW)/src/thermal/load.cpp

OUT := ../public/sim

film: $(SRCS) shims/TimeLib.h
	$(CXX) $(CXXFLAGS) $(SRCS) -lz -o $@

frames: film
	rm -rf $(OUT) && mkdir -p $(OUT)
	./film $(OUT)

clean:
	rm -f film

.PHONY: frames clean
