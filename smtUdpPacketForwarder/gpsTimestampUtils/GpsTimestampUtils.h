#ifndef GPS_TS_UTLIS_H
#define GPS_TS_UTILS_H

#include <cmath>

bool isLeap(unsigned long long gpsTime);
unsigned char countLeaps(unsigned long long gpsTime, bool accumLeaps);
bool isUnixTimeleap(unsigned long long unixTime);
long double unix2gps(long double unixTime);
long double gps2unix(long double gpsTime);

#endif
