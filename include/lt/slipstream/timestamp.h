#pragma once

#include <string>
#include <time.h>

namespace lt::slipstream {

static inline std::string format_timestamp(uint64_t timestamp)
{
    static constexpr size_t timestamp_max = 40;

    std::string output(timestamp_max, ' ');

    static constexpr uint64_t one_billion = 1000000000L;
    time_t epoch_seconds = timestamp / one_billion;
    uint64_t epoch_nanos = timestamp % one_billion;

    size_t n;
    tm tm;
    gmtime_r(&epoch_seconds, &tm);

    n = strftime(&output[0], timestamp_max, "%FT%T", &tm);

    n += snprintf(&output[n], timestamp_max-n, ".%09ld", epoch_nanos);

    //n += strftime(&output[n], timestamp_max-n, "%z", &tm);

    output.resize(n);

    return output;
}

static inline int64_t parse_timestamp(const char * str)
{
    struct tm tm;
    char *remainder;
    int64_t nanos = 0;

    remainder = strptime(str, "%FT%T", &tm);

    if (remainder != NULL) {
        time_t epoch_seconds = mktime(&tm) - timezone;
        if (epoch_seconds == -1) {
            return -1;
        }
        if (remainder[0] == '.') {
            int64_t multiplier = 100000000;
            for (int i=0; i < 9; i++) {
                char c = remainder[i+1];
                if (!c) {
                    break;
                }
                if (c >= '0' && c <= '9') {
                    nanos += (c-'0') * multiplier;
                } else {
                    break;
                }
                multiplier /= 10;
            }
        }
        return nanos + epoch_seconds * 1000000000;
    } else {
        return -1;
    }
}

} // namespace lt::slipstream
