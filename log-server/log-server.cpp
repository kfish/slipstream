#include "lt/slipstream/websockets.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "dimcli/cli.h"

using namespace lt::slipstream;
using namespace std;

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;

    Dim::Cli cli;

    cli.helpNoArgs();

    auto &remix_input_path =
        cli.opt<std::string>("<PATH>")
            .desc("specify input filename");

    auto &remix_channel_names =
        cli.optVec<std::string>("channel c")
            .desc("include frames from the specified channel. this option may be used multiple times to include multiple channels. see below for examples.");

    auto &websockets_location =
            cli.opt<std::string>("websockets w", "/ws")
                    .desc("HTTP location of websockets interface")
                    .valueDesc("PATH");

    auto &http_static_path =
            cli.opt<std::string>("static s", "")
                    .desc("Path to static content to serve over HTTP.")
                    .valueDesc("PATH");

    auto &archive_path =
            cli.opt<std::string>("archive a", "")
                    .desc("Path to slipstream archives.")
                    .valueDesc("PATH");

    auto &http_port = cli.opt<uint16_t>("http-port p", 80)
                            .desc("HTTP port to serve on.")
                            .valueDesc("PORT");

    cli.footer(
        "channel names may be specified matching <app>/<channel>, eg:\n\n"
        "\t-c thanos/log\t include \"log\" cnannel from application \"thanos\".\n"
        "\t-c thanos/*\t include all channels from application \"thanos\".\n"
        "\t-c */log\t include \"log\" channels from all applications.\n"
        "\t-c log\t\t include \"log\" channels from all applications.\n"
    );

    cli.action([&](Dim::Cli &) {
        PathServer<PlainText>(*remix_input_path, *remix_channel_names,
                *websockets_location, *http_static_path, *archive_path, *http_port).run();
        return true;
    });

    cli.exec(std::cerr, argc, argv);
    return cli.exitCode();
}
