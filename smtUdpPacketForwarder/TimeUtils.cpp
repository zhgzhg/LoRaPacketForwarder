#include "TimeUtils.h"
#include <cstdio>
#include <cstring>
#include <mutex>
#include <chrono>

std::mutex tm_mutex;

struct tm* ts_localtime_r(const time_t& timer, struct tm *buf)
{
    const std::lock_guard<std::mutex> lock{tm_mutex};

    struct tm *result{std::localtime(&timer)};

    std::memcpy(buf, result, sizeof(struct tm));

    return buf;
}

const char* ts_asciitime(const time_t& timer, char *buf, size_t buf_sz)
{
    struct tm ltime;
    std::strftime(buf, buf_sz, "%c", ts_localtime_r(timer, &ltime));
    return buf;
}

int iso8601_utc_extended_now(const struct timeval *now, char *result, size_t result_sz)
{
    if (now == nullptr || result == nullptr || result_sz < 21) return -1; // need at least "YYYY-MM-DDTHH:MM:SSZ"

    struct tm tm;
    if (gmtime_r(&now->tv_sec, &tm) == NULL) return -1;

    // Base ISO 8601 time: YYYY-MM-DDTHH:MM:SS
    size_t len = strftime(result, result_sz, "%FT%T", &tm);
    if (len == 0) {
        if (result_sz > 0) result[0] = '\0';
        return -1;
    }

    // Fractional seconds: .uuuuuuZ (total length = 28)
    // support up to micros, although the spec allows even more

    if (len + 8 + 1 > result_sz) {
	if (len + 1 < result_sz) { // fallback to shorter form
            result[len] = 'Z';
            result[len + 1] = '\0';
            return 0;
	}
        result[result_sz - 1] = '\0';
        return -1;
    }

    int n = snprintf(result + len, result_sz - len, ".%06ldZ", (long) now->tv_usec);
    if (n < 0 || (size_t) n >= result_sz - len) {
        result[result_sz - 1] = '\0';
        return -1;
    }

    return 0;
}

time_t add_seconds(const time_t to, int seconds)
{
    struct tm ltime;
    ts_localtime_r(to, &ltime);

    ltime.tm_sec += seconds;
    return mktime(&ltime);
}

uint64_t curr_timestamp_us()
{
  std::chrono::time_point<std::chrono::system_clock> curr = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(curr.time_since_epoch()).count();
}

uint32_t compute_rf_tx_timestamp_correction_us(
  uint32_t fsk_rx_datarate_bauds, uint32_t packet_size, uint32_t spreading_factor,
  uint32_t bandwidth_khz, uint32_t coding_rate, bool is_crc_enabled, bool is_ppm_mode,
  uint32_t spi_freq_hz)
{
    uint32_t offset = 3536U;

    if (fsk_rx_datarate_bauds != 0)
    { return offset + ((uint32_t)680000 / fsk_rx_datarate_bauds) - 20; }

    // get coefficient from the CR 4/<coef> like (4/5, 4/6, 4/7, or 4/8)
    uint32_t cr = coding_rate - 4;

    uint32_t delay_x, bw_pow;

    switch (bandwidth_khz)
    {
        case 125:
            delay_x = 64;
            bw_pow = 1;
            break;
        case 250:
            delay_x = 32;
            bw_pow = 2;
            break;
        case 500:
            delay_x = 16;
            bw_pow = 4;
            break;
        default:
            delay_x = 0;
            bw_pow = 0;
            break;
    }

    uint32_t sz = packet_size;
    uint32_t sf = spreading_factor;
    uint32_t crc_en = is_crc_enabled;
    uint32_t ppm = is_ppm_mode;

    uint32_t delay_y, delay_z;
    uint32_t timestamp_correction;

    /* timestamp correction code, variable delay */
    if ((sf >= 6) && (sf <= 12) && (bw_pow > 0)) {
        if ((2*(sz + 2*crc_en) - (sf-7)) <= 0) { /* payload fits entirely in first 8 symbols */
            delay_y = ( ((1<<(sf-1)) * (sf+1)) + (3 * (1<<(sf-4))) ) / bw_pow;
            delay_z = 32 * (2*(sz+2*crc_en) + 5) / bw_pow;
        } else {
            delay_y = ( ((1<<(sf-1)) * (sf+1)) + ((4 - ppm) * (1<<(sf-4))) ) / bw_pow;
            delay_z = (16 + 4*cr) * (((2*(sz+2*crc_en)-sf+6) % (sf - 2*ppm)) + 1) / bw_pow;
        }
        timestamp_correction = delay_x + delay_y + delay_z;
    } else {
        timestamp_correction = 0;
    }

    long double spi_freq_correction_ns = 1000000000.0 / spi_freq_hz;
    spi_freq_correction_ns *= 2; //(sz + 2);

    return offset + timestamp_correction + (uint32_t)(spi_freq_correction_ns / 1000);
}
