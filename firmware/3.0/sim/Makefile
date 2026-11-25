CXX := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-write-strings -Wno-unused-parameter \
	-Wno-deprecated-declarations -Wno-ignored-qualifiers -Ishims -I../include

SRCS := \
	sim_main.cpp \
	sim_display.cpp \
	sim_stubs.cpp \
	../src/hardware/display/fonts.cpp \
	../src/general/colorschemes.cpp \
	../src/core/tc_thermal_core.cpp \
	../src/gui/gui.cpp \
	../src/gui/buttons.cpp \
	../src/gui/livemode.cpp \
	../src/gui/mainmenu.cpp \
	../src/gui/settingsmenu.cpp \
	../src/gui/videomenu.cpp

TARGET := thermocam_sim

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -lz -o $@

screenshots: $(TARGET)
	./$(TARGET) --screenshot-all out

clean:
	rm -f $(TARGET)
	rm -rf out

.PHONY: all screenshots clean
