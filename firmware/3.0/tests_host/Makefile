CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror -I../src/core -I../include

CORE_SRCS := ../src/core/tc_proto_v2.cpp ../src/core/tc_thermal_core.cpp
TEST_SRCS := test_proto_v2.cpp test_thermal_core.cpp

.PHONY: all test clean

all: run_tests

run_tests: $(CORE_SRCS) $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(CORE_SRCS) $(TEST_SRCS)

test: run_tests
	./run_tests

clean:
	rm -f run_tests
