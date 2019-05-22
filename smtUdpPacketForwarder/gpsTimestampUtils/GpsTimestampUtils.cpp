// inspired from gpstimeutil.js: a javascript library which translates between GPS and unix time
// https://www.gw-openscience.org/static/js/gpstimeutil.js


#include <cmath>

static const unsigned long long GPS_LEAPS[] = { 46828800, 78364801, 109900802,
   173059203, 252028804, 315187205, 346723206, 393984007, 425520008,
   457056009, 504489610, 551750411, 599184012, 820108813, 914803214,
   1025136015, 1119744016, 1167264017
};


// Test to see if a GPS second is a leap second
bool isLeap(unsigned long long gpsTime) {
  for (unsigned char i = 0; i < sizeof(GPS_LEAPS); ++i) {
    if (GPS_LEAPS[i] == gpsTime) return true;
  }
  return false;
}

// Count number of leap seconds that have passed   
// timestamp is in seconds
unsigned char countLeaps(unsigned long long gpsTime, bool accumLeaps) {
  unsigned char nleaps = 0;

  if (accumLeaps) {
    for (unsigned char i = 0; i < sizeof(GPS_LEAPS); ++i) {
 	 if (gpsTime + i >= GPS_LEAPS[i]) {
 	   nleaps += 1;
 	 }
    }
  } else {
    for (unsigned char i = 0; i < sizeof(GPS_LEAPS); ++i) {
 	 if (gpsTime >= GPS_LEAPS[i]) {
 	   nleaps += 1;
 	 }
    }
  }

  return nleaps;
}

// Test to see if a unixtime second is a leap second
bool isUnixTimeleap(unsigned long long unixTime) {
  unsigned long long gpsTime = unixTime - 315964800;
  gpsTime += countLeaps(gpsTime, true) - 1;
  return isLeap(gpsTime);
}

// Convert Unix Time to GPS Time
long double unix2gps(long double unixTime) {
  long double fpart, gpsTime, ipart;

  ipart = std::floor(unixTime);
  fpart = fmod(unixTime, 1);
  gpsTime = ipart - 315964800.0;
  
  if (isUnixTimeleap((unsigned long long)std::ceil(unixTime))) {
    fpart *= 2;
  }

  return gpsTime + fpart + countLeaps(gpsTime, true);
}

// Convert GPS Time to Unix Time
long double gps2unix(long double gpsTime) {
  long double fpart, unixTime, ipart;
  unsigned long long ipartReal;

  fpart = fmod(gpsTime, 1);
  ipart = std::floor(gpsTime);
  ipartReal = (unsigned long long) ipart;
  unixTime = ipart + 315964800 - countLeaps(ipartReal, false);

  if (isLeap(ipartReal + 1)) {
    unixTime = unixTime + fpart / 2;
  } else if (isLeap(ipartReal)) {
    unixTime = unixTime + (fpart + 1) / 2;
  } else {
    unixTime = unixTime + fpart;
  }
  return unixTime;
}
