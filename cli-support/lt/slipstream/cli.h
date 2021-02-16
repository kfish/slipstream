#pragma once

#include <iostream>
#include <string>
#include <thread>

#include "dimcli/cli.h"

#include "lt/slipstream/plaintext.h"
#include "lt/slipstream/filter.h"
#include "lt/slipstream/reader.h"
#include "lt/slipstream/seek.h"
#include "lt/slipstream/writer.h"
#include "lt/slipstream/timestamp.h"

namespace lt::slipstream::cli {

inline void log(const std::string& path, const std::string& application_name,
    const std::string& channel_name)
{
    auto channel_writer =
        ChannelPathWriter<PlainText>(path, application_name, channel_name);

    std::string line = "";

    while (std::getline(std::cin, line)) {
        channel_writer.write(SerialString{line});
    }
}

inline void dump(const std::string& path, const std::vector<std::string>& channel_names, const std::string& start_time, const std::string& end_time, bool follow)
{
    int64_t start = parse_timestamp(start_time.c_str());
    int64_t end = parse_timestamp(end_time.c_str());

    auto channel_reader =
        ChannelPathSeeker<PlainText>(path);

    uint64_t source_timestamp;
    Envelope envelope;
    SerialString s;

    if (start != -1) {
        channel_reader.seek_time(start);
    } else if (follow) {
        channel_reader.seek(0, SEEK_END);
    }

    while(true) {
        if (!channel_reader.read(s, source_timestamp, envelope)) {
            if (follow) {
                usleep(100);
                continue;
            } else {
                break;
            }
        }

        if (end != -1 && source_timestamp > static_cast<uint64_t>(end)) {
            break;
        }

        auto timestamp = format_timestamp(source_timestamp);

        std::cout << timestamp << " " << s << std::endl;
    }
}

template <typename T>
inline void dumpjson(const std::string& path, const std::vector<std::string>& channel_names, const std::string& start_time, const std::string& end_time, bool follow)
{
    int64_t start = parse_timestamp(start_time.c_str());
    int64_t end = parse_timestamp(end_time.c_str());

    auto channel_reader =
        ChannelPathSeeker<T>(path);

    if (start != -1) {
        channel_reader.seek_time(start);
    } else if (follow) {
        channel_reader.seek(0, SEEK_END);
    }

    uint64_t source_timestamp = 0;

    do {
        while(auto o = channel_reader.read_json(source_timestamp)) {

            if (end != -1 && source_timestamp > static_cast<uint64_t>(end)) {
                return;
            }

            std::cout << *o << std::endl;
        }
        usleep(100);
    } while(follow);
}

inline void count(const std::string& path)
{
    auto scanner = PathScanner(path);

    int c = 0;

    do {
        try {
            scanner.next();
            scanner.skip(1);
            c++;
        } catch (const std::exception&) {
            break;
        }
    } while (true);

    std::cout << c << std::endl;
}

inline void remix(const std::vector<std::string>& input_paths, const std::string& output_path,
const std::vector<std::string>& channel_names, const std::string& start_time, const std::string& end_time)
{
    int fd;

    if (output_path == "-") {
        fd = STDOUT_FILENO;
    } else {
        fd = ::open(output_path.c_str(), O_WRONLY | O_CREAT, S_IRUSR|S_IWUSR);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
    }
    auto out = kj::FdOutputStream(kj::AutoCloseFd(fd));

    int64_t start = parse_timestamp(start_time.c_str());
    int64_t end = parse_timestamp(end_time.c_str());

    int c = 0;

    auto filter = Filter(channel_names);

    auto seeker_group = PathSeekerGroup(input_paths);

    auto filter_seeker = FilterSeeker(seeker_group, filter);

    uint64_t source_timestamp;
    Envelope envelope;

    if (start != -1) {
        filter_seeker.seek_time(start);
    }

    while(true) {
        if (!filter_seeker.peek(source_timestamp, envelope)) {
            break;
        }

        if (end != -1 && source_timestamp > static_cast<uint64_t>(end)) {
            break;
        }

        filter_seeker.copy_frame(out);
        c++;

        auto timestamp = format_timestamp(source_timestamp);

        std::cerr << timestamp
            << " " << envelope.identifier
            << " " << envelope.encoding << std::endl;
    }

    std::cerr << c << " frames copied" << std::endl;
}

template <typename T>
int main(int argc, char* argv[])
{
    Dim::Cli cli;

    cli.helpNoArgs();

    auto &logger =
        cli.command("log")
            .desc("Log timestamped plaintext messages from stdin.");

    auto &logger_path =
        logger.opt<std::string>("<PATH>")
            .desc("The file name to write.");

    auto &logger_application_name =
        logger.opt<std::string>("<APPLICATION_NAME>")
            .desc("The application name to log.");

    auto &logger_channel_name =
        logger.opt<std::string>("channel c", "log")
            .desc("The channel name to log.");

    logger.action([&](Dim::Cli &) {
        cli::log(
            *logger_path, *logger_application_name,
            *logger_channel_name);
        return true;
    });

    auto &dumper =
        cli.command("dump")
            .desc("Dump timestamped plaintext messages.");

    auto &dumper_path =
        dumper.opt<std::string>("<PATH>")
            .desc("Specify input filename");

    auto &dumper_channel_names =
        dumper.optVec<std::string>("channel c")
            .desc("Include frames from the specified channel. This option may be used multiple times to include multiple channels. See below for examples.");

    auto &dumper_start =
        dumper.opt<std::string>("start s", "")
            .desc("Include only frames at or after the specified time");

    auto &dumper_end =
        dumper.opt<std::string>("end e", "")
            .desc("Include only frames before the specified time");

    auto &dumper_follow =
        dumper.opt<bool>("f follow")
            .desc("Continue dumping as file grows");

    dumper.action([&](Dim::Cli &) {
        cli::dump(*dumper_path, *dumper_channel_names, *dumper_start, *dumper_end, *dumper_follow);
        return true;
    });

    auto &dumpjson =
        cli.command("json")
            .desc("Dump timestamped plaintext messages as json.");

    auto &dumpjson_path =
        dumpjson.opt<std::string>("<PATH>")
            .desc("The file name to read.");

    auto &dumpjson_channel_names =
        dumpjson.optVec<std::string>("channel c")
            .desc("Include frames from the specified channel. This option may be used multiple times to include multiple channels. See below for examples.");

    auto &dumpjson_start =
        dumpjson.opt<std::string>("start s", "")
            .desc("Include only frames at or after the specified time");

    auto &dumpjson_end =
        dumpjson.opt<std::string>("end e", "")
            .desc("Include only frames before the specified time");

    auto &dumpjson_follow =
        dumpjson.opt<bool>("f follow")
            .desc("Continue dumping as file grows");

    dumpjson.action([&](Dim::Cli &) {
        cli::dumpjson<T>(*dumpjson_path, *dumpjson_channel_names, *dumpjson_start, *dumpjson_end, *dumpjson_follow);
        return true;
    });

    auto &count =
        cli.command("count")
            .desc("Count frames.");

    auto &count_path =
        count.opt<std::string>("<PATH>")
            .desc("The file name to read.");

    count.action([&](Dim::Cli &) {
        cli::count(*count_path);
        return true;
    });

    auto &remix =
        cli.command("remix")
            .desc("Extract selected frames.");

    auto &remix_input_paths =
        remix.optVec<std::string>("[PATH]")
            .desc("Specify input filename(s)");

    auto &remix_output_path =
        remix.opt<std::string>("output o", "-")
            .desc("Specify output filename")
            .defaultDesc("- (stdout)");

    auto &remix_channel_names =
        remix.optVec<std::string>("channel c")
            .desc("Include frames from the specified channel. This option may be used multiple times to include multiple channels. See below for examples.");

    auto &remix_start =
        remix.opt<std::string>("start s", "")
            .desc("Include only frames at or after the specified time");

    auto &remix_end =
        remix.opt<std::string>("end e", "")
            .desc("Include only frames before the specified time");

    remix.footer(
        "Channel names may be specified matching <app>/<channel>, eg:\n\n"
        "\t-c thanos/log\t Include \"log\" cnannel from application \"thanos\".\n"
        "\t-c thanos/*\t Include all channels from application \"thanos\".\n"
        "\t-c */log\t Include \"log\" channels from all applications.\n"
        "\t-c log\t\t Include \"log\" channels from all applications.\n"
    );

    remix.action([&](Dim::Cli &) {
        cli::remix(*remix_input_paths, *remix_output_path, *remix_channel_names, *remix_start, *remix_end);
        return true;
    });


    cli.exec(std::cerr, argc, argv);
    return cli.exitCode();
}

} // namespace lt::slipstream::cli
