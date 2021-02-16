#pragma once

#include <kj/io.h>

#include "lt/slipstream/types.h"
#include "lt/slipstream/writer.h"
#include "lt/slipstream/reader.h"

namespace lt::slipstream::testing {

template <typename T>
class ChannelRoundtrip {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;

    /* Headerless */
    ChannelRoundtrip(
        std::enable_if<std::is_same_v<header_type, lt::slipstream::no_type>, T>* = 0)
    {
        pipe(fds_);

        out_ = std::make_unique<kj::FdOutputStream>(kj::AutoCloseFd(fds_[1]));
        writer_ = std::make_unique<lt::slipstream::ChannelWriter<T>>(
                      out_.get(), application_name_, channel_name_);

        in_ = std::make_unique<kj::FdInputStream>(kj::AutoCloseFd(fds_[0]));
        reader_ = std::make_unique<lt::slipstream::ChannelReader<T>>(in_.get());
    }

    /* Mandatory header */
    ChannelRoundtrip(
        const header_type& header,
        std::enable_if<!std::is_same_v<header_type, lt::slipstream::no_type>, T>* = 0)
    {
        pipe(fds_);

        out_ = std::make_unique<kj::FdOutputStream>(kj::AutoCloseFd(fds_[1]));
        writer_ = std::make_unique<lt::slipstream::ChannelWriter<T>>(
                      out_.get(), application_name_, channel_name_, header);

        in_ = std::make_unique<kj::FdInputStream>(kj::AutoCloseFd(fds_[0]));
        reader_ = std::make_unique<lt::slipstream::ChannelReader<T>>(in_.get());

        /* Check header is same */
        header_type r_header;
        reader_->header(r_header);
        BOOST_CHECK (r_header== header);
    }

    bool roundtrip(const data_type& input)
    {
        writer_->write(input);
        data_type output;
        if (!reader_->read(output)) {
            return false;
        }

        return (output == input);
    }

   private:
    int fds_[2];
    std::unique_ptr<kj::FdInputStream> in_;
    std::unique_ptr<kj::FdOutputStream> out_;
    std::string application_name_ = "test";
    std::string channel_name_ = "log";

    std::unique_ptr<lt::slipstream::ChannelWriter<T>> writer_;
    std::unique_ptr<lt::slipstream::ChannelReader<T>> reader_;
};

template <typename T>
class RC_ChannelRoundtrip {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;

    /* Headerless */
    static void run(int count=1000,
        std::enable_if<std::is_same_v<header_type, lt::slipstream::no_type>, T>* = 0)
    {
        auto rt = ChannelRoundtrip<T>();

        auto g = rc::gen::arbitrary<data_type>();

        for (int i = 0; i < count; ++i) {
            RC_ASSERT(rt.roundtrip(*g));
        }
    }

    /* Mandatory header */
    static void run(const header_type& header, int count=1000,
        std::enable_if<!std::is_same_v<header_type, lt::slipstream::no_type>, T>* = 0)
    {
        auto rt = ChannelRoundtrip<T>(header);

        auto g = rc::gen::arbitrary<data_type>();

        for (int i = 0; i < count; ++i) {
            RC_ASSERT(rt.roundtrip(*g));
        }
    }
};

} // namespace lt::slipstream::testing
