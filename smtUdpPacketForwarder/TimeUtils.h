#ifndef NOWIRINGIPI
  #include <wiringPi.h>
#else
  // covered by RadioLib/src/linux-workarounds/dummy.cpp
  unsigned int millis();
  unsigned int micros();
#endif

#include <ctime>

struct tm* ts_localtime_r(const time_t& timer, struct tm *buf);
const char* ts_asciitime(const time_t& timer, char *buf, size_t buf_sz);
time_t add_seconds(const time_t to, int seconds);

