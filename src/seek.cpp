#include "lt/slipstream/seek.h"

#include <sys/types.h>
#include <unistd.h>

#include "lt/slipstream/seek_time.h"

namespace lt::slipstream {

SeekableStream::~SeekableStream() noexcept(false) {}
FdSeekableStream::~FdSeekableStream() noexcept(false) {}
Seeker::~Seeker() noexcept(false) {}
FdSeeker::~FdSeeker() noexcept(false) {}
PathSeeker::~PathSeeker() noexcept(false) {}
PathSeekerGroup::~PathSeekerGroup() noexcept(false) {}

FdSeekableStream::FdSeekableStream(int fd) : kj::FdInputStream(fd), fd_(fd)
{
}

int64_t FdSeekableStream::seek(int64_t offset, int whence)
{
    return lseek64(fd_, offset, whence);
}

int64_t FdSeekableStream::tell()
{
    return lseek64(fd_, 0, SEEK_CUR);
}

FdSeeker::FdSeeker(int fd) : fd_(fd), fdSeekableStream_(fd),
    scanner_(ScannerWrapper(fdSeekableStream_))
{
}

int64_t FdSeeker::seek(int64_t offset, int whence)
{
    uint64_t ret = fdSeekableStream_.seek(offset, whence);
    scanner_.reset();
    return ret;
}

int64_t FdSeeker::tell()
{
    return fdSeekableStream_.tell() - frame_header_length;
}

bool FdSeeker::seek_time(uint64_t timestamp)
{
    return seek_time_bisect<FdSeeker>(*this, timestamp);
}

void FdSeeker::reset()
{
    return scanner_.reset();
}

bool FdSeeker::next()
{
    return scanner_.next();
}

bool FdSeeker::peek(uint64_t& source_timestamp)
{
    return scanner_.peek(source_timestamp);
}

bool FdSeeker::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    return scanner_.peek(source_timestamp, envelope);
}

size_t FdSeeker::copy_frame(kj::OutputStream& out)
{
    return scanner_.copy_frame(out);
}

size_t FdSeeker::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    return scanner_.tryRead(buffer, minBytes, maxBytes);
}

void FdSeeker::skip(size_t bytes)
{
    scanner_.skip(bytes);
}

PathSeeker::PathSeeker(const std::string& path)
{
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::system_error(errno, std::system_category());
    }
    seeker_ = std::make_unique<FdSeeker>(fd);
}

PathSeeker::PathSeeker(PathSeeker&& other)
    : seeker_(std::move(other.seeker_))
{
}

bool PathSeeker::seek_time(uint64_t timestamp)
{
    return seeker_->seek_time(timestamp);
}

void PathSeeker::reset()
{
    seeker_->reset();
}

bool PathSeeker::next()
{
    return seeker_->next();
}

bool PathSeeker::peek(uint64_t& source_timestamp)
{
    return seeker_->peek(source_timestamp);
}

bool PathSeeker::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    return seeker_->peek(source_timestamp, envelope);
}

size_t PathSeeker::copy_frame(kj::OutputStream& out)
{
    return seeker_->copy_frame(out);
}

size_t PathSeeker::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    return seeker_->tryRead(buffer, minBytes, maxBytes);
}

void PathSeeker::skip(size_t bytes)
{
    seeker_->skip(bytes);
}

PathSeekerGroup::PathSeekerGroup(const std::vector<std::string>& paths)
{
    for (auto&& path : paths) {
        seekers_.emplace_back(path);
    }
    seeker_group_ = std::make_unique<SeekerGroup<PathSeeker>>(seekers_);
}

PathSeekerGroup::PathSeekerGroup(PathSeekerGroup&& other)
    : seekers_(std::move(other.seekers_)),
      seeker_group_(std::move(other.seeker_group_))
{
}

bool PathSeekerGroup::seek_time(uint64_t timestamp)
{
    return seeker_group_->seek_time(timestamp);
}

void PathSeekerGroup::reset()
{
    seeker_group_->reset();
}

bool PathSeekerGroup::next()
{
    return seeker_group_->next();
}

bool PathSeekerGroup::peek(uint64_t& source_timestamp)
{
    return seeker_group_->peek(source_timestamp);
}

bool PathSeekerGroup::peek(uint64_t& source_timestamp, Envelope& envelope)
{
    return seeker_group_->peek(source_timestamp, envelope);
}

size_t PathSeekerGroup::copy_frame(kj::OutputStream& out)
{
    return seeker_group_->copy_frame(out);
}

size_t PathSeekerGroup::tryRead(void* buffer, size_t minBytes, size_t maxBytes)
{
    return seeker_group_->tryRead(buffer, minBytes, maxBytes);
}

void PathSeekerGroup::skip(size_t bytes)
{
    seeker_group_->skip(bytes);
}
} // namespace lt::slipstream
