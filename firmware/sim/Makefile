CXX := clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-write-strings -Wno-unused-parameter \
	-Wno-deprecated-declarations -Wno-ignored-qualifiers -Wno-vla-cxx-extension -Ishims -I../include

COMMON_SRCS := \
	sim_display.cpp \
	sim_stubs.cpp \
	../src/hardware/display/fonts.cpp \
	../src/general/colorschemes.cpp \
	../src/core/tc_thermal_core.cpp \
	../src/gui/gui.cpp \
	../src/gui/buttons.cpp \
	../src/gui/livemode.cpp \
	../src/gui/mainmenu.cpp \
	../src/gui/templimitsmenu.cpp \
	../src/gui/settingsmenu.cpp \
	../src/gui/videomenu.cpp \
	../src/gui/loadmenu.cpp \
	../src/thermal/load.cpp

TARGET := thermocam_sim
TEST_TARGET := thermocam_sim_tests

all: $(TARGET) $(TEST_TARGET)

$(TARGET): sim_main.cpp $(COMMON_SRCS)
	$(CXX) $(CXXFLAGS) sim_main.cpp $(COMMON_SRCS) -lz -o $@

$(TEST_TARGET): sim_tests.cpp $(COMMON_SRCS)
	$(CXX) $(CXXFLAGS) sim_tests.cpp $(COMMON_SRCS) -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

screenshots: $(TARGET)
	./$(TARGET) --screenshot-all out

clean:
	rm -f $(TARGET) $(TEST_TARGET)
	rm -rf out

.PHONY: all test screenshots clean
