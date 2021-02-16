#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main

#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include "lt/slipstream/capnp/test_delta.capnp.h"

#include "lt/slipstream/capnp.h"
#include "lt/slipstream/serialize.h"
#include "lt/slipstream/testing/roundtrip.h"
#include "lt/slipstream/testing/multichannel_roundtrip.h"

namespace lt::slipstream {

class SerialInt64 : public SlipstreamCapnp<SerialInt64, capnp::SerialInt64, SerialInt64> {
   public:
    static constexpr auto encoding = "capnp/int64";

    SerialInt64(int64_t i=0) : i_(i) {}

    int64_t value() const {
        return i_;
    };

    bool operator==(const SerialInt64& other) const {
        return i_ == other.i_;
    }

    friend std::ostream& operator<<(std::ostream& os, const SerialInt64& s) {
        return os << s.i_;
    }

    static SerialInt64 decode(const reader_type& reader)
    {
        return SerialInt64(reader.getValue());
    }

    static void encode(builder_type& builder, const SerialInt64& value)
    {
        builder.setValue(value.value());
    }

   private:
    int64_t i_;
};

class BiasedInt64 {
   public:
    using header_type = SerialInt64;
    using data_type = SerialInt64;

    BiasedInt64(SerialInt64 bias) : bias_(bias)
    {
    }

    const SerialInt64& header() {
        return bias_;
    }

    SerialInt64 encode(const SerialInt64& input) {
        return {input.value() - bias_.value()};
    }

    SerialInt64 decode(const SerialInt64& input) {
        return {input.value() + bias_.value()};
    }

   private:
    SerialInt64 bias_;
};

class BiasedDeltaInt64 {
   public:
    using header_type = SerialInt64;
    using data_type = SerialInt64;
    using delta_type = SerialInt64;

    BiasedDeltaInt64(SerialInt64 bias) : bias_(bias)
    {
    }

    const SerialInt64& header() {
        return bias_;
    }

    SerialInt64 encode(const SerialInt64& input) {
        keyframe_ = input;
        return {input.value() - bias_.value()};
    }

    SerialInt64 decode(const SerialInt64& input) {
        keyframe_ = {input.value() + bias_.value()};
        return keyframe_;
    }

    SerialInt64 encode_delta(const SerialInt64& input) {
        int64_t d = input.value() - keyframe_.value();
        return {d - bias_.value()};
    }

    SerialInt64 decode_delta(const SerialInt64& input) {
        int64_t d = input.value() + keyframe_.value();
        return {d + bias_.value()};
    }

   private:
    SerialInt64 bias_;
    SerialInt64 keyframe_; //unbiased
};


using BiasedInt64Stream = HeaderStream<BiasedInt64>;
using BiasedDeltaInt64ValueStream = HeaderStream<BiasedDeltaInt64>;

using BiasedDeltaInt64Stream = HeaderDeltaStream<BiasedDeltaInt64>;

} // namespace lt::slipstream

using namespace lt::slipstream;
using namespace lt::slipstream::testing;

namespace rc {

template <>
struct Arbitrary<SerialInt64> {
    static Gen<SerialInt64> arbitrary() {
        return gen::construct<SerialInt64>(gen::arbitrary<int64_t>());
    }
};

} // namespace rc
BOOST_AUTO_TEST_CASE(biased_int64_stream_roundtrip_rw)
{
    int64_t header = 100;
    auto rt = ChannelRoundtrip<BiasedInt64Stream>(header);

    BOOST_CHECK(rt.roundtrip(SerialInt64(0)));
    BOOST_CHECK(rt.roundtrip(SerialInt64(77)));
    BOOST_CHECK(rt.roundtrip(SerialInt64(13)));
}

RC_BOOST_PROP(biased_int64_stream_roundtrip_rw_rc, (SerialInt64 header))
{
    RC_ChannelRoundtrip<BiasedInt64Stream>::run(header);
}

RC_BOOST_PROP(biased_delta_int64_value_stream_roundtrip_rw_rc, (SerialInt64 header))
{
    RC_ChannelRoundtrip<BiasedDeltaInt64ValueStream>::run(header);
}

RC_BOOST_PROP(biased_delta_int64_stream_roundtrip_rw_rc, (SerialInt64 header))
{
    RC_ChannelRoundtrip<BiasedDeltaInt64Stream>::run(header);
}

BOOST_AUTO_TEST_CASE(multichannel_biased_int64_stream_roundtrip_rw)
{
    MultiChannelWriter<BiasedInt64Stream>::header_map headers;
    headers.emplace_back("s1", SerialInt64(100));
    headers.emplace_back("s2", SerialInt64(1000));
    auto rt = MultiChannelRoundtrip<BiasedInt64Stream>(headers);

    BOOST_CHECK(rt.roundtrip("s1", SerialInt64(0)));
    BOOST_CHECK(rt.roundtrip("s2", SerialInt64(77)));
    BOOST_CHECK(rt.roundtrip("s1", SerialInt64(13)));
    BOOST_CHECK(rt.roundtrip("s2", SerialInt64(13)));
}

RC_BOOST_PROP(multichannel_biased_int64_stream_roundtrip_rw_rc, ())
{
    MultiChannelWriter<BiasedInt64Stream>::header_map headers;
    headers.emplace_back("s1", SerialInt64(100));
    headers.emplace_back("s2", SerialInt64(1000));
    RC_MultiChannelRoundtrip<BiasedInt64Stream>::run(headers);
}

RC_BOOST_PROP(multichannel_biased_delta_int64_value_stream_roundtrip_rw_rc, ())
{
    MultiChannelWriter<BiasedInt64Stream>::header_map headers;
    headers.emplace_back("s1", SerialInt64(100));
    headers.emplace_back("s2", SerialInt64(1000));
    RC_MultiChannelRoundtrip<BiasedDeltaInt64ValueStream>::run(headers);
}

BOOST_AUTO_TEST_CASE(multichannel_biased_delta_int64_stream_roundtrip_rw)
{
    MultiChannelWriter<BiasedDeltaInt64Stream>::header_map headers;
    headers.emplace_back("s1", SerialInt64(100));
    headers.emplace_back("s2", SerialInt64(1000));
    auto rt = MultiChannelRoundtrip<BiasedDeltaInt64Stream>(headers);

    BOOST_CHECK(rt.roundtrip("s1", SerialInt64(0)));
    BOOST_CHECK(rt.roundtrip("s1", SerialInt64(0)));
    BOOST_CHECK(rt.roundtrip("s1", SerialInt64(0)));
    BOOST_CHECK(rt.roundtrip("s2", SerialInt64(77)));
    BOOST_CHECK(rt.roundtrip("s1", SerialInt64(13)));
    BOOST_CHECK(rt.roundtrip("s2", SerialInt64(13)));
}

RC_BOOST_PROP(multichannel_biased_delta_int64_stream_roundtrip_rw_rc, ())
{
    MultiChannelWriter<BiasedDeltaInt64Stream>::header_map headers;
    headers.emplace_back("s1", SerialInt64(100));
    headers.emplace_back("s2", SerialInt64(1000));
    RC_MultiChannelRoundtrip<BiasedDeltaInt64Stream>::run(headers);
}
