#include <rapidcheck.h>

#include "lt/slipstream/binary.h"

namespace rc {

template <>
struct Arbitrary<lt::slipstream::SerialBinary> {
    static Gen<lt::slipstream::SerialBinary> arbitrary() {
        return gen::construct<lt::slipstream::SerialBinary>(gen::arbitrary<std::vector<uint8_t>>());
    }
};

} // namespace rc

