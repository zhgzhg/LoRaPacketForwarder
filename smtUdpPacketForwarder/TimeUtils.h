#ifndef NOWIRINGIPI
  #include <wiringPi.h>
#else
  // covered by RadioLib/src/linux-workarounds/dummy.cpp
  unsigned int millis();
  unsigned int micros();
#endif

#include <ctime>
#include <cstdint>
#include <type_traits>

struct tm* ts_localtime_r(const time_t& timer, struct tm *buf);

const char* ts_asciitime(const time_t& timer, char *buf, size_t buf_sz);

time_t add_seconds(const time_t to, int seconds);

uint64_t curr_timestamp_us();

template<typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
T diff_timestamps(T now, T future, bool &result_isfutureok)
{
  T diff{};

  if (future >= now)
  { diff = future - now; result_isfutureok = true; }
  else
  { diff = now - future; result_isfutureok = false; }

  return diff;
}

uint32_t compute_rf_tx_timestamp_correction_us(
  uint32_t fsk_rx_datarate_bauds, uint32_t packet_size, uint32_t spreading_factor,
  uint32_t bandwidth_khz, uint32_t coding_rate, bool is_crc_enabled, bool is_ppm_mode);
