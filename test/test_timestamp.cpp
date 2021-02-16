#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main

#include "lt/slipstream/timestamp.h"

#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>

using namespace lt::slipstream;

static void roundtrip(uint64_t timestamp)
{
    std::string str = format_timestamp(timestamp);

    int64_t parsed = parse_timestamp(str.c_str());

    BOOST_CHECK(parsed != -1);

    BOOST_CHECK(timestamp == static_cast<uint64_t>(parsed));
}

BOOST_AUTO_TEST_CASE(timestamp_roundtrip)
{
    roundtrip(1532190706525382048);
}

RC_BOOST_PROP(timestamp_roundtrip_rc, (uint64_t timestamp))
{
    roundtrip(timestamp);
}
