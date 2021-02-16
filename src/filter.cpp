#include "lt/slipstream/filter.h"

namespace lt::slipstream {

FilterScanner::~FilterScanner() noexcept(false) {}
FilterSeeker::~FilterSeeker() noexcept(false) {}

FilterScanner::FilterScanner(Scanner& scanner, Filter& filter)
    : scanner_(scanner), filter_(filter)
{
}

void FilterScanner::reset()
{
    return scanner_.reset();
}

bool FilterScanner::next()
{
    return scanner_.next();
}

bool FilterScanner::peek(uint64_t& source_timestamp)
{
    return scanner_.peek(source_timestamp);
}

bool FilterScanner::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    while(true) {
        if (!scanner_.peek(source_timestamp, envelope)) {
            return false;
        }

        if (filter_.match(envelope)) {
            return true;
        }

        scanner_.skip(1);
    }
}

size_t FilterScanner::copy_frame(kj::OutputStream& out)
{
    return scanner_.copy_frame(out);
}

size_t FilterScanner::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    return scanner_.tryRead(buffer, minBytes, maxBytes);
}

void FilterScanner::skip(size_t bytes)
{
    scanner_.skip(bytes);
}

FilterSeeker::FilterSeeker(Seeker& seeker, Filter& filter)
    : seeker_(seeker), filter_(filter)
{
}

bool FilterSeeker::seek_time(uint64_t timestamp)
{
    return seeker_.seek_time(timestamp);
}

void FilterSeeker::reset()
{
    return seeker_.reset();
}

bool FilterSeeker::next()
{
    return seeker_.next();
}

bool FilterSeeker::peek(uint64_t& source_timestamp)
{
    return seeker_.peek(source_timestamp);
}

bool FilterSeeker::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    while(true) {
        if (!seeker_.peek(source_timestamp, envelope)) {
            return false;
        }

        if (filter_.match(envelope)) {
            return true;
        }

        seeker_.skip(1);
    }
}

size_t FilterSeeker::copy_frame(kj::OutputStream& out)
{
    return seeker_.copy_frame(out);
}

size_t FilterSeeker::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    return seeker_.tryRead(buffer, minBytes, maxBytes);
}

void FilterSeeker::skip(size_t bytes)
{
    seeker_.skip(bytes);
}
} // namespace lt::slipstream
