#pragma once

#include "lt/slipstream/seek.h"

namespace lt::slipstream {

class ChannelFilter {
   public:
    ChannelFilter(const std::string& channel_name) {
        std::size_t slash = channel_name.find("/");
        if (slash == std::string::npos) {
            application_name_ = {};
            channel_name_ = channel_name;
        } else {
            application_name_ = std::string(channel_name, 0, slash);
            channel_name_ = std::string(channel_name, slash+1);
            if (application_name_ == "*") {
                application_name_ = {};
            }
            if (channel_name_ == "*") {
                channel_name_ = {};
            }
        }
    }

    bool match(const Envelope& envelope) const {
        if (application_name_) {
            if (envelope.identifier.application_name != *application_name_) {
                return false;
            }
        }

        if (channel_name_) {
            if (envelope.identifier.channel_name != *channel_name_) {
                return false;
            }
        }

        return true;
    }
   private:
    std::optional<std::string> application_name_;
    std::optional<std::string> channel_name_;

};

class Filter{
   public:
    Filter(const std::vector<std::string>& channel_names)
    {
        for (auto&& c : channel_names) {
            channel_filters_.emplace_back(c);
        }
        if (channel_filters_.empty()) {
            channel_filters_.emplace_back("*/*");
        }
    }

    bool match(const Envelope& envelope)
    {
        return std::any_of(channel_filters_.begin(), channel_filters_.end(),
            [&](const ChannelFilter& cf) {
                return cf.match(envelope);
            });
    };

   private:
    std::vector<ChannelFilter> channel_filters_;
};

class FilterScanner : public Scanner {
   public:
    FilterScanner(Scanner& scanner, Filter& filter);

    KJ_DISALLOW_COPY(FilterScanner);
    virtual ~FilterScanner() noexcept(false);

    // implements Scanner
    void reset() override;

    bool next() override;

    bool peek(uint64_t& source_timestamp) override;

    bool peek(uint64_t& source_timestamp, Envelope& envelope) override;

    size_t copy_frame(kj::OutputStream& out) override;

    // implements InputStream
    size_t tryRead(void* buffer, size_t minBytes, size_t maxBytes) override;

    void skip(size_t bytes) override;

   private:
    Scanner& scanner_;
    Filter filter_;
};

class FilterSeeker : public Seeker {
   public:
    FilterSeeker(Seeker& seeker, Filter& filter);

    KJ_DISALLOW_COPY(FilterSeeker);
    virtual ~FilterSeeker() noexcept(false);

    // implements Seeker
    bool seek_time(uint64_t timestamp) override;

    // implements Scanner
    void reset() override;

    bool next() override;

    bool peek(uint64_t& source_timestamp) override;

    bool peek(uint64_t& source_timestamp, Envelope& envelope) override;

    size_t copy_frame(kj::OutputStream& out) override;

    // implements InputStream
    size_t tryRead(void* buffer, size_t minBytes, size_t maxBytes) override;

    void skip(size_t bytes) override;

   private:
    Seeker& seeker_;
    Filter filter_;
};

} // namespace lt::slipstream
