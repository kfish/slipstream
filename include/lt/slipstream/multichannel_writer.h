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
#include "lt/slipstream/multichannel_variant.h"
#include "lt/slipstream/writer.h"

using namespace lt::core;

namespace lt::slipstream {

template <typename... Ts>
class MultiChannelWriter {
   public:
    using header_type = typename Variant<Ts...>::header_type;
    using data_type = typename Variant<Ts...>::data_type;
    using delta_type = typename Variant<Ts...>::delta_type;
    using header_map = typename Variant<Ts...>::header_map;
    using channel_writer = std::variant<std::monostate, ChannelWriter<Ts>...>;

    MultiChannelWriter(kj::OutputStream * out,
        const std::string& application_name,
        const header_map& channel_headers = {})
        : out_(out), application_name_(application_name)
    {
        for (auto&& [channel_name, header] : channel_headers) {
            if (header) {
                std::visit(
                    [&](auto&& inner_header) {
                        using H = std::decay_t<decltype(inner_header)>;
                        if constexpr (!std::is_same_v<H, no_type>) {
                            channels_.insert_or_assign(channel_name,
                                channel_writer_new_header<H, Ts..., no_type>(out_, application_name_, channel_name, inner_header)
                                );
                        }
                    },
                    *header);
            }
        }
    }

    bool write(const std::string& channel_name, const data_type& data,
        uint64_t source_timestamp=0, bool force_keyframe=false)
    {
        if (source_timestamp == 0) {
            source_timestamp = Stamp::stamp_clock_rt();
        }

        try {
            channel_writer * channel = &channels_.at(channel_name);

            return std::visit(
                [&](auto&& inner_channel, const auto& inner_data) {
                    using C = std::decay_t<decltype(inner_channel)>;
                    using D = std::decay_t<decltype(inner_data)>;
                    if constexpr (std::is_same_v<C, std::monostate> || std::is_same_v<D, std::monostate>) {
                        return false;
                    } else if constexpr (!std::is_same_v<typename C::data_type, D>) {
                        return false;
                    } else {
                        return inner_channel.write(inner_data, source_timestamp, force_keyframe);
                    }
                },
                *channel, data);
        } catch (std::out_of_range& e) {
            return std::visit(
                [&](const auto& inner_data) {
                    using D = std::decay_t<decltype(inner_data)>;
                    if constexpr (std::is_same_v<D, std::monostate>) {
                        return false;
                    } else {
                        channels_.insert_or_assign(channel_name, channel_writer_new_headerless<D, Ts..., no_type>(out_, application_name_, channel_name));
                        channel_writer * channel = &channels_.at(channel_name);
                        auto result = std::visit(
                            [&](auto&& inner_channel) {
                                using C = std::decay_t<decltype(inner_channel)>;
                                if constexpr (std::is_same_v<C, std::monostate>) {
                                    return false;
                                } else if constexpr (!std::is_same_v<typename C::data_type, D>) {
                                    return false;
                                } else {
                                    return inner_channel.write(inner_data, source_timestamp, force_keyframe);
                                }
                            },
                            *channel);
                        return result;
                    }
                },
                data);
        }
    }

    const Identifier identifier(const std::string& channel_name) const {
        return std::visit([&](auto&& inner_channel) -> Identifier {
            using C = std::decay_t<decltype(inner_channel)>;
            if constexpr (std::is_same_v<C, std::monostate>) {
                throw std::bad_variant_access();
            } else {
                return inner_channel.identifier(channel_name);
            }
        }, channels_.at(channel_name));
    }

   private:
    kj::OutputStream * out_;
    const std::string application_name_;
    std::unordered_map<std::string, channel_writer> channels_;

    template<typename H, typename U, typename... Us>
    channel_writer channel_writer_new_header(kj::OutputStream * out, const std::string& application_name, const std::string& channel_name, const H& header)
    {
        if constexpr (std::is_same_v<U, no_type>) {
            throw std::runtime_error("MCWriter: could not find header type " + std::string(typeid(H).name()) + " for " + application_name + "/" + channel_name);
        } else if constexpr (!std::is_same_v<typename U::header_type, no_type> && std::is_same_v<typename U::header_type, H>) {
            return std::variant<std::monostate, ChannelWriter<Ts>...>(ChannelWriter<U>(out, application_name, channel_name, header));
        } else {
            return channel_writer_new_header<H, Us...>(out, application_name, channel_name, header);
        }
    }

    template<typename D, typename U, typename... Us>
    channel_writer channel_writer_new_headerless(kj::OutputStream * out, const std::string& application_name, const std::string& channel_name)
    {
        if constexpr (std::is_same_v<U, no_type>) {
            throw std::runtime_error("MCWriter: could not match data type " + std::string(typeid(D).name()) + " for " + application_name + "/" + channel_name);
        } else if constexpr (!std::is_same_v<U, std::monostate> && std::is_same_v<typename U::data_type, D>) {
            if constexpr (!std::is_same_v<typename U::header_type, no_type>) {
                throw std::runtime_error("MCWriter: could not match data type " + std::string(typeid(D).name()) + " for " + application_name + "/" + channel_name);
            } else {
                return std::variant<std::monostate, ChannelWriter<Ts>...>(ChannelWriter<U>(out, application_name, channel_name));
            }
        } else {
            return channel_writer_new_headerless<D, Us...>(out, application_name, channel_name);
        }
    }

};

template <typename... Ts>
class MultiChannelPathWriter {
   public:
    using header_type = typename Variant<Ts...>::header_type;
    using data_type = typename Variant<Ts...>::data_type;
    using header_map = typename Variant<Ts...>::header_map;

    MultiChannelPathWriter(const std::string& path,
        const std::string& application_name,
        const header_map& channel_headers = {})
    {
        int fd = open(path.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_NONBLOCK, S_IRUSR|S_IWUSR|S_IRGRP);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
        out_ = std::make_unique<kj::FdOutputStream>(kj::AutoCloseFd(fd));
        channel_writer_ = std::make_unique<MultiChannelWriter<Ts...>>(out_.get(), application_name, channel_headers);
    }

    bool write(const std::string& channel_name, const data_type& data,
        uint64_t source_timestamp=0, bool force_keyframe=false)
    {
        return channel_writer_->write(channel_name, data, source_timestamp, force_keyframe);
    }

   private:
    std::unique_ptr<kj::FdOutputStream> out_;
    std::unique_ptr<MultiChannelWriter<Ts...>> channel_writer_;
};

} // namespace lt::slipstream
