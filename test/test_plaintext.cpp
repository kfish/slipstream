#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main


#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>

#include "lt/slipstream/testing/roundtrip.h"
#include "lt/slipstream/testing/multichannel_roundtrip.h"
#include "lt/slipstream/testing/rc_plaintext.h"

using namespace lt::slipstream;
using namespace lt::slipstream::testing;

BOOST_AUTO_TEST_CASE(plaintext_roundtrip_rw)
{
    auto rt = ChannelRoundtrip<PlainText>();

    BOOST_CHECK(rt.roundtrip({""}));
    BOOST_CHECK(rt.roundtrip({"Hey there"}));
    BOOST_CHECK(rt.roundtrip({"Ho there"}));
    BOOST_CHECK(rt.roundtrip({"How are you doing there?"}));
}

RC_BOOST_PROP(plaintext_roundtrip_rw_rc, ())
{
    RC_ChannelRoundtrip<PlainText>::run();
}

BOOST_AUTO_TEST_CASE(plaintext_multichannel_roundtrip_rw)
{
    using S = lt::slipstream::SerialString;

    auto rt = MultiChannelRoundtrip<PlainText>();

    BOOST_CHECK(rt.roundtrip("log1", S{""}));
    BOOST_CHECK(rt.roundtrip("log2", S{"Hey there"}));
    BOOST_CHECK(rt.roundtrip("log1", S{"Ho there"}));
    BOOST_CHECK(rt.roundtrip("log1", S{"How are you doing there?"}));
}

RC_BOOST_PROP(plaintext_multichannel_roundtrip_rw_rc, ())
{
    RC_MultiChannelRoundtrip<PlainText>::run();
}
