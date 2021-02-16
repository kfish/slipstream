#pragma once

#include "lt/slipstream/timestamp.h"

namespace lt::slipstream::json {

inline std::string enclose_quotes(const std::string& s) {
    return "\"" + s + "\"";
}

inline std::string kv(const std::string& key, const std::string& value) {
    return enclose_quotes(key) + ":" + value;
}

inline std::string record(const std::vector<std::string>& kvs) {
    std::string s = "{";
    auto it = kvs.begin();
    do {
        s += *it;
        ++it;
        if (it != kvs.end()) {
            s += ",";
        }
    } while (it != kvs.end());
    s += "}";

    return s;
}

inline std::string frame(const std::string& data_json,
    uint64_t& source_timestamp, Envelope& envelope)
{
    auto timestamp = format_timestamp(source_timestamp);
    uint64_t epoch_millis = source_timestamp / static_cast<uint64_t>(1000000);
    int nanos = source_timestamp % static_cast<uint64_t>(1000000);

    return record({
        kv("timestamp", enclose_quotes(timestamp)),
        kv("epochMillis", std::to_string(epoch_millis)),
        kv("nanos", std::to_string(nanos)),
        kv("host", enclose_quotes(envelope.identifier.host_name)),
        kv("app", enclose_quotes(envelope.identifier.application_name)),
        kv("channel", enclose_quotes(envelope.identifier.channel_name)),
        kv("data", data_json)});
}

} // namespace lt::slipstream::json
