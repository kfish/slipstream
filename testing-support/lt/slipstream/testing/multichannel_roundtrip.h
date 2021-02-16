#pragma once

#include <string>
#include <kj/io.h>

#include "lt/slipstream/types.h"
#include "lt/slipstream/multichannel_writer.h"
#include "lt/slipstream/multichannel_reader.h"

#include "lt/slipstream/testing/rc_variant.h"

namespace lt::slipstream::testing {

template <typename... Ts>
class MultiChannelRoundtrip {
   public:
    using header_type = typename lt::slipstream::Variant<Ts...>::header_type;
    using data_type = typename lt::slipstream::Variant<Ts...>::data_type;
    using header_map = std::vector<std::pair<std::string, std::optional<header_type>>>;

    MultiChannelRoundtrip(const header_map& channel_headers = {})
    {
        pipe(fds_);

        out_ = std::make_unique<kj::FdOutputStream>(kj::AutoCloseFd(fds_[1]));
        writer_ = std::make_shared<lt::slipstream::MultiChannelWriter<Ts...>>(
                      out_.get(), application_name_, channel_headers);

        in_ = std::make_unique<kj::FdInputStream>(kj::AutoCloseFd(fds_[0]));
        reader_ = std::make_shared<lt::slipstream::MultiChannelReader<Ts...>>(in_.get());
    }

    bool roundtrip(const std::string& channel_name, const data_type& input)
    {
        writer_->write(channel_name, input, 0, false);
        data_type output;
        uint64_t timestamp;
        lt::slipstream::Envelope envelope;
        if (!reader_->read(output, timestamp, envelope)) {
            return false;
        }

        return (output == input && envelope.identifier.channel_name == channel_name);
    }

    bool check_headers(const header_map& channel_headers)
    {
        for (auto&& [channel_name, header] : channel_headers) {
            if constexpr (!std::is_same_v<std::decay_t<decltype(*writer_)>, std::monostate>) {
                auto identifier = writer_->identifier(channel_name);
                if (header) {
                    header_type r_header;
                    if (reader_->header(identifier, r_header)) {
                        std::visit([&](auto&& inner_header, auto&& inner_r_header) {
                            if constexpr (!std::is_same_v<std::decay_t<decltype(inner_header)>, std::monostate> &&
                                          !std::is_same_v<std::decay_t<decltype(inner_header)>, lt::slipstream::no_type> &&
                                          !std::is_same_v<std::decay_t<decltype(inner_r_header)>, std::monostate> &&
                                          !std::is_same_v<std::decay_t<decltype(inner_r_header)>, lt::slipstream::no_type>) {
                                BOOST_CHECK (inner_header == inner_r_header);
                            }
                        },
                        *header, r_header);
                    }
                }
            }
        }

        return true;
    }

   private:
    int fds_[2];
    std::unique_ptr<kj::FdInputStream> in_;
    std::unique_ptr<kj::FdOutputStream> out_;
    std::string application_name_ = "test";

    std::shared_ptr<lt::slipstream::MultiChannelWriter<Ts...>> writer_;
    std::shared_ptr<lt::slipstream::MultiChannelReader<Ts...>> reader_;
};

template <typename... Ts>
class RC_MultiChannelRoundtrip {
   public:
    using header_type = typename lt::slipstream::Variant<Ts...>::header_type;
    using data_type = typename lt::slipstream::Variant<Ts...>::data_type;
    using header_map = std::vector<std::pair<std::string, std::optional<header_type>>>;

    /* Mandatory header */
    static void run(const header_map channel_headers={}, int count=1000)
    {
        int n_variants = std::variant_size_v<data_type>;

        auto rt = MultiChannelRoundtrip<Ts...>(channel_headers);

        // For each data type, have a list of channel names.
        // Use those provided in the header_map if available,
        // otherwise generate some (for non-header streams)
        auto channel_names = std::vector<std::vector<std::string>>(n_variants, std::vector<std::string>());

        for (auto&& [channel_name, header] : channel_headers) {
            if (header) {
                channel_names[header->index()].push_back(channel_name);
            }
        }

        // skip index 0 (std::monostate)
        for (int i=1; i < n_variants; ++i) {
            if (channel_names[i].empty()) {
                for (int j = 1; j <= 8; ++j) {
                    std::string name = "channel-" + std::to_string(i) + "." + std::to_string(j);
                    channel_names[i].push_back(name);
                }
            }
        }

        auto g = rc::gen::arbitrary<data_type>();
        data_type v;

        for (int i = 0; i < count; ++i) {
            // Generate a non-monostate value
            do {
                v = *g;
            } while (!v.index());

            auto variant_index = v.index();
            auto c = rc::gen::inRange(0, static_cast<int>(channel_names[variant_index].size()));
            int channel_index = *c;

            auto channel_name = channel_names[variant_index][channel_index];
            std::visit([&](auto&& inner_data) {
                if constexpr (!std::is_same_v<std::decay_t<decltype(inner_data)>, std::monostate>) {
                    RC_ASSERT(rt.roundtrip(channel_name, v));
                }
            }, v);
        }

        RC_ASSERT(rt.check_headers(channel_headers));
    }
};

} // namespace lt::slipstream::testing
