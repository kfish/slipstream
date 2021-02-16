#include "lt/slipstream/scanner.h"

namespace lt::slipstream {

PeekStream::~PeekStream() noexcept(false) {}
Scanner::~Scanner() noexcept(false) {}
ScannerWrapper::~ScannerWrapper() noexcept(false) {}
PathScanner::~PathScanner() noexcept(false) {}
PathScannerGroup::~PathScannerGroup() noexcept(false) {}

PeekStream::PeekStream(kj::InputStream& inner)
    : inner_(inner), recording_(false)
{
}

void PeekStream::start_recording() {
    write_offset_ = 0;
    read_offset_ = 0;
    recording_ = true;
}

void PeekStream::cancel_recording() {
    write_offset_ = 0;
    read_offset_ = 0;
    recording_ = false;
}

void PeekStream::stop_recording_rewind() {
    read_offset_ = 0;
    recording_ = false;
}

size_t PeekStream::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    if (recording_) {
        size_t nread = inner_.tryRead(buffer, minBytes, maxBytes);
        memcpy(&buffer_[write_offset_], buffer, nread);
        write_offset_ += nread;
        return nread;
    } else {
        uint8_t * b = static_cast<uint8_t*>(buffer);
        size_t cached = std::min(maxBytes, write_offset_ - read_offset_);
        if (cached > 0) {
            memcpy(b, &buffer_[read_offset_], cached);
            read_offset_ += cached;
            b += cached;
            minBytes -= cached;
            minBytes = std::max((size_t)0, minBytes);
            maxBytes -= cached;
            maxBytes = std::max((size_t)0, maxBytes);
        }
        return cached + inner_.tryRead(b, minBytes, maxBytes);
    }
}

void PeekStream::skip(size_t bytes)
{
    cancel_recording();
    return inner_.skip(bytes);
}

ScannerWrapper::ScannerWrapper(kj::InputStream& inner)
    : inner_(PeekStream(inner)), checksum_(0), at_eof_(false)
{
    reset();
}

uint8_t ScannerWrapper::advance() {
    uint8_t cur = checksum_ & 0xFF;

    checksum_ >>= 8;

    if (at_eof_ && --buffered_ > 0) {
        return cur;
    }

    try {
        uint8_t b8;
        inner_.read(&b8, 1);

        uint32_t b32 = b8;
        checksum_ += (b32 << 16);

        buffered_++;
        buffered_ = std::min(buffered_, 3);
    } catch (const std::exception& e) {
        if (at_eof_) {
            throw e;
        } else {
            at_eof_ = true;
        }
    }

    return cur;
}

void ScannerWrapper::reset() {
    checksum_ = 0;
    buffered_ = 0;
    at_eof_ = false;
    source_timestamp_ = 0;
    envelope_length_ = 0;
    next();
}

void ScannerWrapper::soft_reset() {
    inner_.cancel_recording();

    while (buffered_ < 3) {
        advance();
    }

    source_timestamp_ = 0;
    envelope_length_ = 0;
}

bool ScannerWrapper::next() {
    soft_reset();

    while (checksum_ != marker_checksum_) {
        try {
            advance();
        } catch (std::exception&) {
            return false;
        }
    }

    return true;
}

size_t ScannerWrapper::copy_frame(kj::OutputStream& out)
{
    size_t n = 0;

    soft_reset();

    while (true) {
        try {
            uint8_t cur = advance();
            out.write(&cur, 1);
            ++n;
            if (checksum_ == marker_checksum_) {
                break;
            }
        } catch (const std::exception&) {
            break;
        }
    }

    return n;
}

bool ScannerWrapper::peek(uint64_t& source_timestamp)
{
    if (source_timestamp_) {
        source_timestamp = source_timestamp_;
        return true;
    }

    InputStream& in = *this;

    inner_.start_recording();

    Framing framing;

    auto result = framing.read(in);
    if (!result.first) {
        inner_.cancel_recording();
        return false;
    }

    source_timestamp_ = framing.source_timestamp;
    envelope_length_ = framing.envelope_length;

    source_timestamp = source_timestamp_;

    return true;
}

bool ScannerWrapper::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    InputStream& in = *this;

    if (source_timestamp_ == 0) {
        if (!peek(source_timestamp)) {
            return false;
        }
    }

    source_timestamp = source_timestamp_;

    if (!envelope.read(in, envelope_length_)) {
        inner_.cancel_recording();
        return false;
    }

    inner_.stop_recording_rewind();

    buffered_ = 3;

    return true;
}


size_t ScannerWrapper::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    size_t nread = 0;
    uint8_t * b = static_cast<uint8_t*>(buffer);

    while (buffered_ > 0 && maxBytes > 0) {
        switch (buffered_) {
            case 1:
                *b++ = (checksum_ & 0xFF0000) >> 16;
                break;
            case 2:
                *b++ = (checksum_ & 0xFF00) >> 8;
                break;
            default:
                *b++ = checksum_ & 0xFF;
                break;
        }
        nread++;
        buffered_--;
        minBytes--;
        maxBytes--;
    }

    nread += inner_.tryRead(b, minBytes, maxBytes);

    return nread;
}

void ScannerWrapper::skip(size_t bytes)
{
    if (bytes <= static_cast<size_t>(buffered_)) {
        buffered_ -= bytes;
    } else {
        inner_.skip(bytes - buffered_);
        buffered_ = 0;
    }

    source_timestamp_ = 0;
    envelope_length_ = 0;
}

PathScanner::PathScanner(const std::string& path)
{
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::system_error(errno, std::system_category());
    }
    in_ = std::make_unique<kj::FdInputStream>(kj::AutoCloseFd(fd));
    scanner_ = std::make_unique<ScannerWrapper>(*in_);
}

PathScanner::PathScanner(PathScanner&& other)
    : in_(std::move(other.in_)), scanner_(std::move(other.scanner_))
{
}

void PathScanner::reset()
{
    scanner_->reset();
}

bool PathScanner::next()
{
    return scanner_->next();
}

bool PathScanner::peek(uint64_t& source_timestamp)
{
    return scanner_->peek(source_timestamp);
}

bool PathScanner::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    return scanner_->peek(source_timestamp, envelope);
}

size_t PathScanner::copy_frame(kj::OutputStream& out)
{
    return scanner_->copy_frame(out);
}

size_t PathScanner::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    return scanner_->tryRead(buffer, minBytes, maxBytes);
}

void PathScanner::skip(size_t bytes)
{
    scanner_->skip(bytes);
}

PathScannerGroup::PathScannerGroup(const std::vector<std::string>& paths)
{
    for (auto&& path : paths) {
        scanners_.emplace_back(path);
    }
    scanner_group_ = std::make_unique<ScannerGroup<PathScanner>>(scanners_);
}

PathScannerGroup::PathScannerGroup(PathScannerGroup&& other)
    : scanners_(std::move(other.scanners_)),
      scanner_group_(std::move(other.scanner_group_))
{
}

void PathScannerGroup::reset()
{
    scanner_group_->reset();
}

bool PathScannerGroup::next()
{
    return scanner_group_->next();
}

bool PathScannerGroup::peek(uint64_t& source_timestamp)
{
    return scanner_group_->peek(source_timestamp);
}

bool PathScannerGroup::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    return scanner_group_->peek(source_timestamp, envelope);
}

size_t PathScannerGroup::copy_frame(kj::OutputStream& out)
{
    return scanner_group_->copy_frame(out);
}

size_t PathScannerGroup::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    return scanner_group_->tryRead(buffer, minBytes, maxBytes);
}

void PathScannerGroup::skip(size_t bytes)
{
    scanner_group_->skip(bytes);
}

} // namespace lt::slipstream

