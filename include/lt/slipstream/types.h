#pragma once

#include <variant>

namespace lt::slipstream {

struct no_type {
};

struct PayloadHeader {
    bool operator==(const PayloadHeader& other) const
    {
        return true;
    }
};

struct PayloadKeyframe {
    bool operator==(const PayloadKeyframe& other) const
    {
        return true;
    }
};
struct PayloadDelta {
    bool operator==(const PayloadDelta& other) const
    {
        return true;
    }
};

typedef std::variant<PayloadKeyframe, PayloadDelta> PayloadData;
typedef std::variant<PayloadHeader, PayloadData> PayloadKind;

} // namespace lt::slipstream
