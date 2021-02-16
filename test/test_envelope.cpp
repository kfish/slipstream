#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main

#include "lt/slipstream/envelope.h"

#include <boost/test/unit_test.hpp>
#include <rapidcheck/boost_test.h>

using namespace lt::slipstream;

class CountedOutputStream : public kj::OutputStream
{
   public:
    CountedOutputStream(kj::OutputStream& inner) : inner_(inner) {}

    size_t count() const { return count_; }

    void reset() { count_ = 0; }

    void write(const void* buffer, size_t size) override
    {
        count_ += size;
        inner_.write(buffer, size);
    }

   private:
    kj::OutputStream& inner_;
    size_t count_{0};
};


static void roundtrip_rw(const Envelope& input)
{
    int fds[2];
    Envelope output;

    pipe(fds);

    auto inner = kj::FdOutputStream(kj::AutoCloseFd(fds[1]));
    auto out = CountedOutputStream(inner);
    //auto&& [ write_success, nwritten ] = input.write(out);
    //
    //BOOST_CHECK(write_success);

    input.write(out);

    auto in = kj::FdInputStream(kj::AutoCloseFd(fds[0]));
    //auto&& [ read_success, nread ] = output.read(in);
    auto read_success = output.read(in, input.size());

    BOOST_CHECK(read_success);

    //BOOST_CHECK(nread == nwritten);

    BOOST_CHECK(input == output);

    BOOST_CHECK(input.size() == output.size());

    BOOST_CHECK(out.count() == input.size());
}

BOOST_AUTO_TEST_CASE(envelope_roundtrip_rw)
{
    roundtrip_rw(Envelope{Identifier{"foo", "bar", "baz"}, "text/plain", PayloadHeader{}});
}

static void roundtrip_rw_rc(const Envelope& input)
{
    int fds[2];
    Envelope output;

    pipe(fds);

    auto inner = kj::FdOutputStream(kj::AutoCloseFd(fds[1]));
    auto out = CountedOutputStream(inner);
    // auto&& [ write_success, nwritten ] = input.write(out);
    //
    //RC_ASSERT(write_success);

    input.write(out);

    auto in = kj::FdInputStream(kj::AutoCloseFd(fds[0]));
    //auto&& [ read_success, nread ] = output.read(in);
    auto read_success = output.read(in, input.size());

    RC_ASSERT(read_success);

    //RC_ASSERT(nread == nwritten);

    RC_ASSERT(input == output);

    RC_ASSERT(input.size() == output.size());

    RC_ASSERT(out.count() == input.size());
}

RC_BOOST_PROP(envelope_roundtrip_rw_rc, (std::string h, std::string a, std::string c, std::string e)) {
    const auto p = *rc::gen::inRange(0, 2);
    PayloadKind payload_kind = PayloadHeader{};
    if (p == 1) {
        payload_kind = PayloadKeyframe{};
    } else if (p == 2) {
        payload_kind = PayloadDelta{};
    }
    roundtrip_rw_rc(Envelope{Identifier{h, a, c}, e, payload_kind});
}
