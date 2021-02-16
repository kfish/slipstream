#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main

#include "lt/slipstream/framing.h"

#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>

using namespace lt::slipstream;

#if 0
static void dump(const char * label, const Framing& rf)
{
    printf ("%s Framing: e:%d\tp:%d\tt:%ld\tsync:%s\n", label,
        rf.envelope_length, rf.payload_length, rf.source_timestamp,
        rf.sync ? "true" : "false");
}

static void manual_check(const Framing& input, const Framing& output)
{
    if (input.envelope_length != output.envelope_length) {
        printf ("Envelope lengths mismatch: %d vs %d\n",
        input.envelope_length, output.envelope_length);
    }
    if (input.payload_length != output.payload_length) {
        printf ("Payload lengths mismatch: %d vs %d\n",
        input.payload_length, output.payload_length);
    }
    if (input.source_timestamp != output.source_timestamp) {
        printf ("Source timestamps mismatch\n");
    }
    if (input.sync != output.sync) {
        printf ("Sync mismatch\n");
    }
}
#endif

static void roundtrip_encdec(const Framing& input)
{
    uint8_t buf[sizeof(Framing)];
    Framing output;

    input.encode(buf);
    BOOST_CHECK(output.decode(buf));

    BOOST_CHECK(input == output);
}

BOOST_AUTO_TEST_CASE(framing_encdec_roundtrip)
{
    roundtrip_encdec(Framing{0, 0, 0, false});
    roundtrip_encdec(Framing{0, 0, 0, true});

    roundtrip_encdec(Framing{13, 0, 0, false});
    roundtrip_encdec(Framing{13, 0, 0, true});

    roundtrip_encdec(Framing{7, 300, 1234567, false});
    roundtrip_encdec(Framing{7, 300, 1234567, true});

    roundtrip_encdec(Framing{(1<<12)-1, (1<<20)-1, std::numeric_limits<uint64_t>::max(), false});
    roundtrip_encdec(Framing{(1<<12)-1, (1<<20)-1, std::numeric_limits<uint64_t>::max(), true});

}

static void roundtrip_rw(const Framing& input)
{
    int fds[2];
    Framing output;

    pipe(fds);

    auto out = kj::FdOutputStream(kj::AutoCloseFd(fds[1]));
    auto&& [ write_success, nwritten ] = input.write(out);

    BOOST_CHECK(write_success);

    auto in = kj::FdInputStream(kj::AutoCloseFd(fds[0]));
    auto&& [ read_success, nread ] = output.read(in);

    BOOST_CHECK(read_success);

    BOOST_CHECK(nread == nwritten);

    BOOST_CHECK(input == output);
}

BOOST_AUTO_TEST_CASE(framing_roundtrip_rw)
{
    roundtrip_rw(Framing{0, 0, 0, 0, false});
    roundtrip_rw(Framing{0, 0, 0, 0, true});

    roundtrip_rw(Framing{13, 0, 0, 0, false});
    roundtrip_rw(Framing{13, 0, 0, 0, true});

    roundtrip_rw(Framing{7, 300, 1234567, 0x8f31, false});
    roundtrip_rw(Framing{7, 300, 1234567, 0x8f31, true});

    roundtrip_rw(Framing{(1<<12)-1, (1<<20)-1, std::numeric_limits<uint64_t>::max(), 0xFFFF, false});
    roundtrip_rw(Framing{(1<<12)-1, (1<<20)-1, std::numeric_limits<uint64_t>::max(), 0xFFFF, true});

}

namespace rc {

template <>
struct Arbitrary<Framing> {
    static Gen<Framing> arbitrary() {
        return gen::build<Framing>(
            gen::set(&Framing::envelope_length, gen::inRange(0, (1<<12)-1)),
            gen::set(&Framing::payload_length, gen::inRange(0, (1<<20)-1)),
            gen::set(&Framing::source_timestamp),
            gen::set(&Framing::checksum),
            gen::set(&Framing::sync));
    }
};

} // namespace rc

static void roundtrip_encdec_rc(const Framing& input)
{
    uint8_t buf[sizeof(Framing)];
    Framing output;

    input.encode(buf);
    RC_ASSERT(output.decode(buf));

    RC_ASSERT(input == output);
}

RC_BOOST_PROP(framing_roundtrip_encdec_rc, (Framing framing)) {
    roundtrip_encdec_rc(framing);
}

static void roundtrip_rw_rc(const Framing& input)
{
    int fds[2];
    Framing output;

    pipe(fds);

    auto out = kj::FdOutputStream(kj::AutoCloseFd(fds[1]));
    auto&& [ write_success, nwritten ] = input.write(out);

    RC_ASSERT(write_success);

    auto in = kj::FdInputStream(kj::AutoCloseFd(fds[0]));
    auto&& [ read_success, nread ] = output.read(in);

    RC_ASSERT(read_success);

    RC_ASSERT(nread == nwritten);

    RC_ASSERT(input == output);
}

RC_BOOST_PROP(framing_roundtrip_rw_rc, (Framing framing)) {
    roundtrip_rw_rc(framing);
}
