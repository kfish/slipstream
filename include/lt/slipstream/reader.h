#pragma once

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <optional>
#include <system_error>
#include <kj/io.h>

#include "lt/slipstream/envelope.h"
#include "lt/slipstream/framing.h"
#include "lt/slipstream/json.h"

namespace lt::slipstream {

template <typename T>
class ChannelReader {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;
    using delta_type = typename T::delta_type;

    ChannelReader(kj::InputStream * in)
        : in_(in)
    {
        if constexpr (std::is_same_v<header_type, no_type>) {
            thang_ = std::make_unique<T>();
        } else {
            Framing framing;

            auto result = framing.read(*in_);
            if (!result.first) {
                throw std::runtime_error("Bad frame read");
            }

            Envelope envelope;
            if (!envelope.read(*in_, framing.envelope_length)) {
                throw std::runtime_error("Bad envelope read");
            }

            if (!T::has_header_encoding(envelope.encoding)) {
                throw std::runtime_error("Wrong encoding");
            }

            header_type header;
            if (!T::read_header(*in_, header, framing.payload_length)) {
                throw std::runtime_error("Bad header read");
            }

            thang_ = std::make_unique<T>(header);
        }
    }

    /* Mandatory header */
    ChannelReader(kj::InputStream * in, header_type& header,
        std::enable_if<!std::is_same_v<header_type, no_type>, T>* = 0)
        : in_(in)
    {
        thang_ = std::make_unique<T>(header);
    }

    ChannelReader& operator=(ChannelReader&& other)
    {
        in_ = std::move(other.in_);
        thang_ = std::move(other.thang_);

        return *this;
    }

    ChannelReader(ChannelReader&& other)
    {
        *this = std::move(other);
    }

    bool read(data_type& data, uint64_t& source_timestamp,
        Envelope& envelope)
    {
        Framing framing;

        auto result = framing.read(*in_);
        if (!result.first) {
            return false;
        }

        source_timestamp = framing.source_timestamp;

        if (!envelope.read(*in_, framing.envelope_length)) {
            return false;
        }

        if (!T::has_encoding(envelope.encoding)) {
            return false;
        }

        if (auto pd = std::get_if<PayloadData>(&envelope.payload_kind)) {
            if (std::holds_alternative<PayloadKeyframe>(*pd)) {
                try {
                    auto result = thang_->read(*in_, data, framing.payload_length);
                    return result;
                } catch (std::exception&) {
                    return false;
                }
            } else if constexpr (!std::is_same_v<delta_type, no_type>) {
                try {
                    return thang_->read_delta(*in_, data, framing.payload_length);
                } catch (std::exception&) {
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    bool read_header_internal(header_type& header, size_t length)
    {
        try {
            auto result = thang_->read_header(*in_, header, length);
            return result;
        } catch (std::exception&) {
            return false;
        }
    }

    bool read_internal(data_type& data, size_t length)
    {
        try {
            auto result = thang_->read(*in_, data, length);
            return result;
        } catch (std::exception&) {
            return false;
        }
    }

    bool read_delta_internal(data_type& data, size_t length)
    {
        if constexpr (!std::is_same_v<delta_type, no_type>) {
            try {
                return thang_->read_delta(*in_, data, length);
            } catch (std::exception&) {
                return false;
            }
        } else {
            return false;
        }
    }

    bool read(data_type& data)
    {
        uint64_t source_timestamp;
        Envelope envelope;
        return read(data, source_timestamp, envelope);
    }

    const std::optional<std::string> read_json(uint64_t& source_timestamp)
    {
        Envelope envelope;
        data_type data;

        if (read(data, source_timestamp, envelope)) {
            auto data_json = thang_->to_json(data);
            return json::frame(data_json, source_timestamp, envelope);
        } else {
            return {};
        }
    }

    bool header(header_type& header) {
        if constexpr (std::is_same_v<header_type, no_type>) {
            return false;
        } else if (thang_ == nullptr) {
            return false;
        } else {
            header = thang_->header();
            return true;
        }
    }

   private:
    kj::InputStream * in_;
    std::unique_ptr<T> thang_;
};

template <typename T>
class ChannelPathReader {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;

    ChannelPathReader(const std::string& path)
    {
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
        in_ = std::make_unique<kj::FdInputStream>(kj::AutoCloseFd(fd));
        channel_reader_ = std::make_unique<ChannelReader<T>>(in_.get());
    }

    bool read(data_type& data, uint64_t& source_timestamp,
        Envelope& envelope)
    {
        return channel_reader_->read(data, source_timestamp, envelope);
    }

    const std::optional<std::string> read_json(uint64_t& source_timestamp)
    {
        return channel_reader_->read_json(source_timestamp);
    }

    const header_type * header() {
        return channel_reader_->header();
    }

   private:
    std::unique_ptr<kj::FdInputStream> in_;
    std::unique_ptr<ChannelReader<T>> channel_reader_;
};


} // namespace lt::slipstream
