#pragma once
#include <cstdint>
using time_t = long;
inline int hour() { return 14; }
inline int minute() { return 32; }
inline int second() { return 8; }
inline int day() { return 25; }
inline int month() { return 11; }
inline int year() { return 2026; }
inline time_t now() { return 1795617128L; }
inline void setTime(int, int, int, int, int, int) {}
struct SimTeensyClock { void set(time_t) {} time_t get() { return now(); } };
extern SimTeensyClock Teensy3Clock;
