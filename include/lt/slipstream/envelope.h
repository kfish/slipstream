#pragma once

#include <iostream>

#include <functional>
#include <string>
#include <kj/io.h>

#include "lt/slipstream/types.h"

namespace lt::slipstream {

struct Identifier {
    std::string host_name;
    std::string application_name;
    std::string channel_name;

    bool operator==(const Identifier& other) const
    {
        return
            host_name == other.host_name &&
            application_name == other.application_name &&
            channel_name == other.channel_name;
    }

    std::string to_string() const {
        return host_name + "/" + application_name + "/" + channel_name;
    }

    friend std::ostream& operator<<(std::ostream& os, const Identifier& identifier) {
        return os << identifier.to_string();
    }
};

struct Envelope {
    Identifier identifier;

    std::string encoding;
    PayloadKind payload_kind;

    bool operator==(const Envelope& other) const
    {
        return
            identifier == other.identifier &&
            encoding == other.encoding &&
            payload_kind == other.payload_kind;
    }

    size_t size() const;

    void write(kj::OutputStream& out) const;

    bool read(kj::InputStream& in, size_t length);
};

} // namespace lt::slipstream

namespace std {

template <>
struct hash<lt::slipstream::Identifier>
{
    std::size_t operator()(const lt::slipstream::Identifier& identifier) const
    {
        using std::size_t;
        using std::hash;
        using std::string;

        return ((hash<string>()(identifier.host_name)
                ^ (hash<string>()(identifier.application_name) << 1)) >> 1)
                ^ (hash<string>()(identifier.channel_name) << 1);
    }
};

} // namespace std
