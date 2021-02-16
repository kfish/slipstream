#include <rapidcheck.h>

#include "lt/slipstream/plaintext.h"

namespace rc {

template <>
struct Arbitrary<lt::slipstream::SerialString> {
    static Gen<lt::slipstream::SerialString> arbitrary() {
        return gen::construct<lt::slipstream::SerialString>(
            gen::arbitrary<std::string>()
        );
    }
};

} // namespace rc
