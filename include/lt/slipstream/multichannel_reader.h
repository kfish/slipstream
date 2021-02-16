#pragma once

#include <optional>
#include <unordered_map>

#include "lt/slipstream/reader.h"
#include "lt/slipstream/multichannel_variant.h"

namespace lt::slipstream {

template <typename... Ts>
class MultiChannelReader {
   public:
    using header_type = typename Variant<Ts...>::header_type;
    using data_type = typename Variant<Ts...>::data_type;
    using delta_type = typename Variant<Ts...>::delta_type;
    using channel_reader = std::variant<std::monostate, ChannelReader<Ts>...>;

    MultiChannelReader(kj::InputStream * in)
        : in_(in)
    {
    }

    bool read(data_type& data, uint64_t& source_timestamp,
        Envelope& envelope)
    {
        while (true) {
            Framing framing;

            auto result = framing.read(*in_);
            if (!result.first) {
                return false;
            }

            source_timestamp = framing.source_timestamp;

            if (!envelope.read(*in_, framing.envelope_length)) {
                return false;
            }

            if (auto pd = std::get_if<PayloadData>(&envelope.payload_kind)) {
                channel_reader * channel = nullptr;

                try {
                    channel = &channels_.at(envelope.identifier);
                } catch (std::out_of_range& e) {
                    channels_.insert_or_assign(envelope.identifier,
                        channel_reader_new_headerless<Ts..., no_type>(in_, envelope.encoding));

                    channel = &channels_.at(envelope.identifier);
                }

                if (std::holds_alternative<PayloadKeyframe>(*pd)) {
                    bool result = std::visit(
                        [&](auto&& inner_channel) -> bool {
                            using C = std::decay_t<decltype(inner_channel)>;
                            if constexpr (std::is_same_v<C, std::monostate>) {
                                return false;
                            } else {
                                typename C::data_type inner_data;
                                bool result = inner_channel.read_internal(inner_data, framing.payload_length);
                                data = inner_data;
                                return result;
                            }
                        },
                        *channel);
                    return result;
                } else if constexpr (!std::is_same_v<delta_type, no_type>) {
                    bool result = std::visit(
                        [&](auto&& inner_channel) -> bool {
                            using C = std::decay_t<decltype(inner_channel)>;
                            if constexpr (std::is_same_v<C, std::monostate>) {
                                return false;
                            } else {
                                typename C::data_type inner_data;
                                bool result = inner_channel.read_delta_internal(inner_data, framing.payload_length);
                                data = inner_data;
                                return result;
                            }
                        },
                        *channel);
                    return result;
                } else {
                    return false;
                }
            } else {
                if (channels_.find(envelope.identifier) == channels_.end()) {
                    header_type header;
                    channels_.insert_or_assign(envelope.identifier,
                        channel_reader_new_header<header_type, Ts..., no_type>(in_, envelope.encoding, header, framing.payload_length));
                }
            }
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
            // visit data, call its to_json()
            auto data_json =  std::visit(
                [](auto&& inner_data) {
                    return decltype(inner_data)::to_json(inner_data);
                },
                data);

            return json::frame(data_json, source_timestamp, envelope);
        } else {
            return {};
        }
    }

    bool header(const Identifier& identifier, header_type& header) {
        if constexpr (std::is_same_v<header_type, no_type>) {
            return false;
        } else {
            try {
                return std::visit([&](auto&& inner_channel) -> bool {
                    if constexpr (std::is_same_v<std::decay_t<decltype(inner_channel)>, std::monostate>) {
                        throw std::bad_variant_access();
                    } else {
                        using H = typename std::decay_t<decltype(inner_channel)>::header_type;
                        if constexpr (std::is_same_v<H, std::monostate> || std::is_same_v<H, no_type>) {
                            return false;
                        } else {
                            H inner_header;
                            inner_channel.header(inner_header);
                            header = inner_header;
                            return true;
                        }
                    }
                }, channels_.at(identifier));
            } catch (std::out_of_range&) {
                return false;
            }
        }
    }

   private:
    kj::InputStream * in_;
    std::unordered_map<Identifier, channel_reader> channels_;

    template<typename U, typename... Us>
    channel_reader channel_reader_new_headerless(kj::InputStream * in, const std::string& encoding)
    {
        if constexpr (std::is_same_v<U, no_type>) {
            throw std::runtime_error("MCReader: Unknown headerless content-type " + encoding);
        } else {
            if (U::has_encoding(encoding)) {
                return ChannelReader<U>(in);
            } else {
                return channel_reader_new_headerless<Us...>(in, encoding);
            }
        }
    }

    template<typename H>
    channel_reader channel_reader_new_header(kj::InputStream * in, const std::string& encoding, H& header, size_t length)
    {
        throw std::runtime_error("MCReader: Unknown header content-type " + encoding);
    }

    template<typename H, typename U, typename... Us>
    channel_reader channel_reader_new_header(kj::InputStream * in, const std::string& encoding, H& header, size_t length)
    {
        if constexpr (std::is_same_v<U, no_type>) {
            throw std::runtime_error("MCReader: Unknown header content-type " + encoding);
        } else if constexpr (!std::is_same_v<U, std::monostate> && !std::is_same_v<typename U::header_type, no_type>) {
            if (U::has_header_encoding(encoding)) {
                typename U::header_type inner_header;

                if (!U::read_header(*in_, inner_header, length)) {
                    throw std::runtime_error("Bad header read");
                }

                channel_reader reader = ChannelReader<U>(in, inner_header);
                header = inner_header;
                return reader;
            }
        }

        return channel_reader_new_header<H, Us...>(in, encoding, header, length);
    }

};

template <typename... Ts>
class MultiChannelPathReader {
   public:
    using header_type = typename Variant<Ts...>::header_type;
    using data_type = typename Variant<Ts...>::data_type;

    MultiChannelPathReader(const std::string& path)
    {
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
        in_ = std::make_unique<kj::FdInputStream>(kj::AutoCloseFd(fd));
        channel_reader_ = std::make_unique<MultiChannelReader<Ts...>>(*in_);
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

    const header_type * header(const Identifier& identifier) {
        return channel_reader_->header(identifier);
    }

   private:
    std::unique_ptr<kj::FdInputStream> in_;
    std::unique_ptr<MultiChannelReader<Ts...>> channel_reader_;
};

} // namespace lt::slipstream
