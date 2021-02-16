#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main


#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>

#include "lt/slipstream/testing/multichannel_roundtrip.h"

#include "lt/slipstream/testing/rc_binary.h"
#include "lt/slipstream/testing/rc_plaintext.h"

using namespace lt::slipstream;
using namespace lt::slipstream::testing;

BOOST_AUTO_TEST_CASE(variant_multichannel_roundtrip_rw)
{
    using B = lt::slipstream::SerialBinary;
    using S = lt::slipstream::SerialString;

    auto rt = MultiChannelRoundtrip<Binary, PlainText>();

    BOOST_CHECK(rt.roundtrip("bin1", B{}));
    BOOST_CHECK(rt.roundtrip("log1", S{""}));
    BOOST_CHECK(rt.roundtrip("bin2", B{0x00, 0x07}));
    BOOST_CHECK(rt.roundtrip("log2", S{"Hey there"}));
    BOOST_CHECK(rt.roundtrip("bin1", B{0x00, 0x01, 0xFF, 0xFE, 0xED}));
    BOOST_CHECK(rt.roundtrip("log1", S{"Ho there"}));
    BOOST_CHECK(rt.roundtrip("log1", S{"How are you doing there?"}));
    BOOST_CHECK(rt.roundtrip("bin1", B(std::vector<uint8_t>(1024, 0x00))));
}

RC_BOOST_PROP(variant_multichannel_roundtrip_rw_rc, ())
{
    RC_MultiChannelRoundtrip<Binary, PlainText>::run();
}
