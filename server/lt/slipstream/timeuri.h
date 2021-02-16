#pragma once

#include "lt/slipstream/seek.h"
#include "lt/slipstream/timestamp.h"

#include "dynamicpage.h"
#include "uri.h"

size_t streamlen(std::shared_ptr<std::istream> stream) {
    stream->seekg(0, stream->end);
    size_t len = stream->tellg();
    stream->seekg(0, stream->beg);
    return len;
}

template <typename... Ts>
class SlipstreamTimeURI : public DynamicPageHandler {
   public:
    SlipstreamTimeURI(const std::vector<std::string>& static_paths,
        const std::string& archive_path)
        : DynamicPageHandler(static_paths)
        , archive_path_(archive_path)
    {
    }

    std::shared_ptr<seasocks::Response> handle_dynamic(const seasocks::Request& request) {
        using namespace seasocks;
        using namespace lt::slipstream;

        auto localUri = LocalUri(request.getRequestUri());

        auto qss = std::stringstream();
        for (auto&& [q, v] : localUri.getQueryParams()) {
            qss << "(" << q << "=" << v << ") ";
        }

        std::string json;
        try {
            auto path = archive_path_ + localUri.path();
            auto channel_reader = ChannelPathSeeker<Ts...>(path);

            int64_t start = -1;
            for (auto&& [q, v] : localUri.getQueryParams()) {
                if (q == "t") {
                    start = parse_timestamp(v.c_str());
                }
            }
            if (start != -1) {
                channel_reader.seek_time(start);
            }

            uint64_t source_timestamp;

            auto o = channel_reader.read_json(source_timestamp);
            if (o) {
                json = *o;
            } else {
                json = "Can't read";
            }
        } catch (std::exception&) {
            json = "Not found";
        }

        auto ss = std::make_shared<std::stringstream>();
        (*ss) << json;

        auto headers = SimpleResponse::Headers({
            {"Content-Type", "application/json"},
            {"Content-Length", std::to_string(streamlen(ss))},
            {"Connection", "keep-alive"},
        });

        const auto response = std::make_shared<SimpleResponse>(
            ResponseCode::Ok,  // response code
            ss,                // stream
            headers,           // headers
            true,              // keep alive
            true               // flush instantly
        );
        return response;
    }

   private:
    std::string archive_path_;
};
