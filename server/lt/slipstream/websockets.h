#include "seasocks/PrintfLogger.h"
#include "seasocks/Server.h"
#include "seasocks/StringUtil.h"
#include "seasocks/WebSocket.h"
#include "seasocks/util/Json.h"

#include <set>
#include <thread>

#include "lt/slipstream/filter.h"
#include "lt/slipstream/reader.h"
#include "lt/slipstream/seek.h"
#include "lt/slipstream/plaintext.h"

#include "timeuri.h"

using namespace seasocks;

class MyHandler: public WebSocket::Handler {
public:
    explicit MyHandler(Server* server) : _server(server) {
    }

    virtual void onConnect(WebSocket* connection) {
        _connections.insert(connection);
        std::cout << "Connected: " << connection->getRequestUri()
                  << " : " << formatAddress(connection->getRemoteAddress())
                  << std::endl;
        std::cout << "Credentials: " << *(connection->credentials()) << std::endl;
    }

    virtual void onDisconnect(WebSocket* connection) {
        _connections.erase(connection);
        std::cout << "Disconnected: " << connection->getRequestUri()
                  << " : " << formatAddress(connection->getRemoteAddress())
                  << std::endl;
    }

    void send(std::string& s) {
        for (auto c : _connections) {
            c->send(s.c_str());
        }

        std::cout << s << std::endl;
    }

private:
    std::set<WebSocket*> _connections;
    Server* _server;
};

template <typename... Ts>
class PathServer {
   public:
    PathServer(const std::string& path,
        const std::vector<std::string>& channel_names,
        const std::string &websockets_location,
        const std::string &http_static_path,
        const std::string &archive_path,
        uint16_t http_port) :
        channel_reader_(path),
        channel_names_(channel_names),
        websockets_location_(websockets_location),
        http_static_path_(http_static_path),
        archive_path_(archive_path),
        http_port_(http_port)
    {
    }

    void run() {
        channel_reader_.seek(-4096, SEEK_END);

        //auto filter = Filter(channel_names_);

        //auto filter_seeker = FilterSeeker(channel_reader_, filter);

        // Set up seasocks server
        auto logger = std::make_shared<PrintfLogger>(Logger::Level::DEBUG);

        Server server(logger);

        std::vector<std::string> static_paths =
            { "/", "/index.html", "/scripts/*", "/images/*", "/assets/*", "/lt/*" };

        server.addPageHandler(std::make_shared<SlipstreamTimeURI<Ts...>>(static_paths, archive_path_));

        auto handler = std::make_shared<MyHandler>(&server);
        server.addWebSocketHandler(websockets_location_.c_str(), handler);

        // Start the server
        std::thread server_thread ( [&]{
            server.serve(http_static_path_.c_str(), http_port_);
        });

        uint64_t source_timestamp;
        static constexpr int fps = 30; // frames per second
        static constexpr int frame_delay_ms = 1000 / fps;

        while(true) {
            server.execute([&]{
                std::optional<std::string> o[2];
                int i=0;

                while (o[i] = channel_reader_.read_json(source_timestamp)) {
                    i = 1-i;
                }

                i = 1-i;

                if (o[i]) {
                    handler->send(*o[i]);
                }
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));
        };
    }

   private:
    lt::slipstream::ChannelPathSeeker<Ts...> channel_reader_;
    const std::vector<std::string>& channel_names_;
    const std::string &websockets_location_;
    const std::string &http_static_path_;
    const std::string &archive_path_;
    uint16_t http_port_;
};

