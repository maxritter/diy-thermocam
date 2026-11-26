#pragma once
// Film override of firmware/sim/shims/TimeLib.h: the clock ticks with the film (g_film_clock_s).
#include <cstdint>
using time_t = long;
extern long g_film_clock_s;  // seconds since midnight
inline int hour() { return static_cast<int>((g_film_clock_s / 3600) % 24); }
inline int minute() { return static_cast<int>((g_film_clock_s / 60) % 60); }
inline int second() { return static_cast<int>(g_film_clock_s % 60); }
inline int day() { return 26; }
inline int month() { return 11; }
inline int year() { return 2026; }
inline time_t now() { return 1795651200L + g_film_clock_s; }
inline void setTime(int, int, int, int, int, int) {}
struct SimTeensyClock { void set(time_t) {} time_t get() { return now(); } };
extern SimTeensyClock Teensy3Clock;
