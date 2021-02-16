#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <memory>
#include <system_error>
#include <unistd.h>

#include "lt/core/stamp.h"

#include "lt/slipstream/envelope.h"
#include "lt/slipstream/framing.h"

using namespace lt::core;

namespace lt::slipstream {

template <typename T>
class ChannelWriter {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;
    using delta_type = typename T::delta_type;

    /* Headerless */
    ChannelWriter(kj::OutputStream * out,
        const std::string& application_name,
        const std::string& channel_name,
        std::enable_if<std::is_same_v<header_type, no_type>, T>* = 0)
        : out_(out),
          envelope_(Envelope{Identifier{"", application_name, channel_name},
                    "", PayloadKeyframe{} }),
          thang_()
    {
        set_hostname();
    }

    /* Mandatory header */
    ChannelWriter(kj::OutputStream * out,
        const std::string& application_name,
        const std::string& channel_name,
        const header_type& header,
        std::enable_if<!std::is_same_v<header_type, no_type>, T>* = 0)
        : out_(out),
          envelope_(Envelope{
            Identifier{"", application_name, channel_name},
            "", PayloadKeyframe{} }),
          thang_(header)
    {
        set_hostname();

        // Write header
        uint32_t envelope_size = envelope_.size();
        uint32_t header_size = thang_.size_header();

        auto framing = Framing {envelope_size, header_size, 0, 0, false};
        framing.write(*out_);

        envelope_.encoding = T::header_encoding(header);
        envelope_.payload_kind = PayloadHeader{};
        envelope_.write(*out_);

        if (!thang_.write_header(*out_)) {
            throw std::system_error(errno, std::system_category());
        }

        envelope_.encoding = T::data_type::encoding;
    }

    ChannelWriter& operator=(ChannelWriter&& other)
    {
        out_ = std::move(other.out_);
        envelope_ = std::move(other.envelope_);
        thang_ = std::move(other.thang_);

        return *this;
    }

    ChannelWriter(ChannelWriter&& other)
        : thang_(std::move(other.thang_))
    {
        *this = std::move(other);
    }

    bool write(const data_type& data, uint64_t source_timestamp=0,
        bool force_keyframe=false)
    {
        if (source_timestamp == 0) {
            source_timestamp = Stamp::stamp_clock_rt();
        }

        uint32_t payload_size = 0;

        if constexpr (std::is_same_v<delta_type, no_type>) {
            payload_size = thang_.size(data);
            envelope_.payload_kind = PayloadKeyframe{};
            envelope_.encoding = T::encoding(data);
            do_keyframe_ = true;
        } else {
            do_keyframe_ = !do_keyframe_; // Alternate since previous frame
            if (force_keyframe) {
                do_keyframe_ = true;
            }
            if (do_keyframe_) {
                payload_size = thang_.size(data);
                envelope_.payload_kind = PayloadKeyframe{};
                envelope_.encoding = T::encoding(data);
            } else {
                //throw std::runtime_error("Writing delta!");
                payload_size = thang_.size_delta(data);
                envelope_.payload_kind = PayloadDelta{};
                envelope_.encoding = T::delta_encoding(data);
            }
        }

        uint32_t envelope_size = envelope_.size();
        auto framing = Framing {envelope_size, payload_size, source_timestamp, 0, false};
        framing.write(*out_);

        envelope_.write(*out_);

        if (do_keyframe_) {
            return thang_.write(*out_, data);
        } else if constexpr (!std::is_same_v<delta_type, no_type>) {
            return thang_.write_delta(*out_, data);
        } else {
            return false;
        }
    }

    const Identifier identifier(const std::string& channel_name) const {
        return envelope_.identifier;
    }

   private:
    kj::OutputStream * out_;
    Envelope envelope_;
    T thang_;
    bool do_keyframe_;

    void set_hostname()
    {
        static char buf[HOST_NAME_MAX];
        gethostname(buf, HOST_NAME_MAX);
        envelope_.identifier.host_name = std::string(buf);
    }
};

template <typename T>
class ChannelPathWriter {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;

    ChannelPathWriter(const std::string& path,
        const std::string& application_name,
        const std::string& channel_name,
        std::enable_if<std::is_same_v<header_type, no_type>, T>* = 0)
    {
        int fd = open(path.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_NONBLOCK, S_IRUSR|S_IWUSR|S_IRGRP);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
        out_ = std::make_unique<kj::FdOutputStream>(kj::AutoCloseFd(fd));
        channel_writer_ = std::make_unique<ChannelWriter<T>>(out_.get(), application_name, channel_name);
    }

    ChannelPathWriter(const std::string& path,
        const std::string& application_name,
        const std::string& channel_name,
        const header_type& header,
        std::enable_if<!std::is_same_v<header_type, no_type>, T>* = 0)
    {
        int fd = open(path.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_NONBLOCK, S_IRUSR|S_IWUSR|S_IRGRP);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
        out_ = std::make_unique<kj::FdOutputStream>(kj::AutoCloseFd(fd));
        channel_writer_ = std::make_unique<ChannelWriter<T>>(out_.get(), application_name, channel_name, header);
    }

    bool write(const data_type& data, uint64_t source_timestamp=0, bool force_keyframe=false)
    {
        return channel_writer_->write(data, source_timestamp, force_keyframe);
    }

   private:
    std::unique_ptr<kj::FdOutputStream> out_;
    std::unique_ptr<ChannelWriter<T>> channel_writer_;
};

} // namespace lt::slipstream
