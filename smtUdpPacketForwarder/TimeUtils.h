#ifndef NOWIRINGIPI
  #include <wiringPi.h>
#else
  // covered by RadioLib/src/linux-workarounds/dummy.cpp
  unsigned int millis();
  unsigned int micros();
#endif