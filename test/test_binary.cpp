#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main


#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>

#include "lt/slipstream/testing/roundtrip.h"
#include "lt/slipstream/testing/multichannel_roundtrip.h"

#include "lt/slipstream/testing/rc_binary.h"

using namespace lt::slipstream;
using namespace lt::slipstream::testing;

BOOST_AUTO_TEST_CASE(binary_roundtrip_rw)
{ auto rt = ChannelRoundtrip<Binary>();

    BOOST_CHECK(rt.roundtrip({}));
    BOOST_CHECK(rt.roundtrip({0x00, 0x07}));
    BOOST_CHECK(rt.roundtrip({0x00, 0x01, 0xFF, 0xFE, 0xED}));
    BOOST_CHECK(rt.roundtrip(std::vector<uint8_t>(1024, 0x00)));
}

RC_BOOST_PROP(binary_roundtrip_rw_rc, ())
{
    RC_ChannelRoundtrip<Binary>::run();
}

BOOST_AUTO_TEST_CASE(binary_multichannel_roundtrip_rw)
{
    using B = lt::slipstream::SerialBinary;

    auto rt = MultiChannelRoundtrip<Binary>();

    BOOST_CHECK(rt.roundtrip("bin1", B{}));
    BOOST_CHECK(rt.roundtrip("bin2", B{0x00, 0x07}));
    BOOST_CHECK(rt.roundtrip("bin1", B{0x00, 0x01, 0xFF, 0xFE, 0xED}));
    BOOST_CHECK(rt.roundtrip("bin1", B(std::vector<uint8_t>(1024, 0x00))));
}

RC_BOOST_PROP(binary_multichannel_roundtrip_rw_rc, ())
{
    RC_MultiChannelRoundtrip<Binary>::run({});
}
