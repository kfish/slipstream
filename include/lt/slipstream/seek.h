#pragma once

#include "lt/slipstream/scanner.h"

namespace lt::slipstream {

class SeekableStream {
   public:
    virtual ~SeekableStream() noexcept(false);

    virtual int64_t seek(int64_t offset, int whence) = 0;

    virtual int64_t tell() = 0;
};

class FdSeekableStream : public kj::FdInputStream, public SeekableStream {
   public:
    FdSeekableStream(int fd);

    virtual ~FdSeekableStream() noexcept(false);

    // implements SeekableStream
    int64_t seek(int64_t offset, int whence) override;

    int64_t tell() override;

    // implements InputStream
    using kj::FdInputStream::tryRead;
    using kj::FdInputStream::skip;

   private:
    int fd_;
};

class Seeker : public Scanner {
   public:
    virtual ~Seeker() noexcept(false);

    virtual bool seek_time(uint64_t timestamp) = 0;
};

class FdSeeker : public SeekableStream, public Seeker {
    // Implements Seeker in terms of FdSeekableStream

   public:
    FdSeeker(int fd);

    virtual ~FdSeeker() noexcept(false);

    // implements SeekableStream
    int64_t seek(int64_t offset, int whence) override;

    int64_t tell() override;

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
    int fd_;
    FdSeekableStream fdSeekableStream_;
    ScannerWrapper scanner_;
};

class PathSeeker : public Seeker {
   public:

    PathSeeker(const std::string& path);

    PathSeeker(PathSeeker&& ps);

    KJ_DISALLOW_COPY(PathSeeker);
    virtual ~PathSeeker() noexcept(false);

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
    std::unique_ptr<FdSeeker> seeker_;
};

template <typename T>
class SeekerGroup : public Seeker {
   public:
    SeekerGroup(std::vector<T>& seekers) : seekers_(seekers), sorted_(false)
    {
        for (size_t ix=0; ix < seekers.size(); ++ix) {
            ixs_.push_back(ix);
        }
    }

    KJ_DISALLOW_COPY(SeekerGroup);
    virtual ~SeekerGroup() noexcept(false) {}

    // implements Seeker
    bool seek_time(uint64_t timestamp)
    {
        for (auto&& s : seekers_) {
            s.seek_time(timestamp);
        }

        return true;
    }

    // implements Scanner
    void reset()
    {
        for (auto&& s : seekers_) {
            s.reset();
        }
    }

    bool next()
    {
        return head(false).next();
    }

    bool peek(uint64_t& source_timestamp) override
    {
        return head().peek(source_timestamp);
    }

    bool peek(uint64_t& source_timestamp, Envelope& envelope) override
    {
        while (!head().peek(source_timestamp, envelope)) {
            ixs_.erase(ixs_.begin());
            if (ixs_.empty()) {
                return false;
            }
        }

        return true;
    }

    size_t copy_frame(kj::OutputStream& out) override
    {
        return head(false).copy_frame(out);
    }

    // implements InputStream
    size_t tryRead(void* buffer, size_t minBytes, size_t maxBytes)
    {
        return head(false).tryRead(buffer, minBytes, maxBytes);
    }

    void skip(size_t bytes)
    {
        head(false).skip(bytes);
    }

   private:
    std::vector<T>& seekers_;
    std::vector<int> ixs_;
    bool sorted_;

    void sort() {
        if (sorted_ || ixs_.empty()) {
            return;
        }

        std::sort(ixs_.begin(), ixs_.end(),
            [&](int i1, int i2) {
                uint64_t t1, t2;

                if (!seekers_[i1].peek(t1)) {
                    return false;
                }
                if (!seekers_[i2].peek(t2)) {
                    return true;
                }
                return t1 < t2;
            });

        sorted_ = true;
    }

    T& head(bool stay_sorted=true) {
        sort();
        sorted_ = stay_sorted;
        return seekers_[ixs_[0]];
    }
};

class PathSeekerGroup : public Seeker {
   public:

    PathSeekerGroup(const std::vector<std::string>& paths);

    PathSeekerGroup(PathSeekerGroup&& other);

    KJ_DISALLOW_COPY(PathSeekerGroup);
    virtual ~PathSeekerGroup() noexcept(false);

    // implements Seeker
    bool seek_time(uint64_t timestamp) override;

    // implements Seeker
    void reset() override;

    bool next() override;

    bool peek(uint64_t& source_timestamp) override;

    bool peek(uint64_t& source_timestamp, Envelope& envelope) override;

    size_t copy_frame(kj::OutputStream& out) override;

    // implements InputStream
    size_t tryRead(void* buffer, size_t minBytes, size_t maxBytes) override;

    void skip(size_t bytes) override;

   private:
    std::vector<PathSeeker> seekers_;
    std::unique_ptr<SeekerGroup<PathSeeker>> seeker_group_;
};

template <typename T>
class ChannelPathSeeker {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;

    ChannelPathSeeker(const std::string& path)
    {
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::system_error(errno, std::system_category());
        }
        seeker_ = std::make_unique<FdSeeker>(fd);
        channel_reader_ = std::make_unique<ChannelReader<T>>(seeker_.get());
    }

    // implements SeekableStream
    int64_t seek(int64_t offset, int whence)
    {
        return seeker_->seek(offset, whence);
    }

    int64_t tell()
    {
        return seeker_->tell();
    }

    bool seek_time(uint64_t timestamp)
    {
        return seeker_->seek_time(timestamp);
    }

    bool read(data_type& data, uint64_t& source_timestamp,
        Envelope& envelope)
    {
        return channel_reader_->read(data, source_timestamp, envelope);
    }

    const std::optional<std::string> read_json(uint64_t& source_timestamp)
    {
        return channel_reader_->read_json(source_timestamp);
    }

    const header_type * header() {
        return channel_reader_->header();
    }

    // implements Scanner
    void reset()
    {
        return seeker_->reset();
    }

    bool next()
    {
        return seeker_->next();
    }

    bool peek(uint64_t& source_timestamp)
    {
        return seeker_->peek(source_timestamp);
    }

    bool peek(uint64_t& source_timestamp, Envelope& envelope)
    {
        return seeker_->peek(source_timestamp, envelope);
    }

    size_t copy_frame(kj::OutputStream& out)
    {
        return seeker_->copy_frame(out);
    }

   private:
    std::unique_ptr<FdSeeker> seeker_;
    std::unique_ptr<ChannelReader<T>> channel_reader_;
};

} // namespace lt::slipstream
