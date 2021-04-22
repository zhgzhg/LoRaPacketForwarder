#include "TimeUtils.h"
#include <cstring>
#include <mutex>

std::mutex tm_mutex;

struct tm* ts_localtime_r(const time_t& timer, struct tm *buf)
{
    const std::lock_guard<std::mutex> lock{tm_mutex};

    struct tm *result{std::localtime(&timer)};

    std::memcpy(buf, result, sizeof(struct tm ));

    return buf;
}

const char* ts_asciitime(const time_t& timer, char *buf, size_t buf_sz)
{
    struct tm ltime;
    std::strftime(buf, buf_sz, "%c", ts_localtime_r(timer, &ltime));
    return buf;
}

time_t add_seconds(const time_t to, int seconds)
{
    struct tm ltime;
    ts_localtime_r(to, &ltime);

    ltime.tm_sec += seconds;
    return mktime(&ltime);
}