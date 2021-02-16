#pragma once

#include "lt/slipstream/reader.h"

#include <algorithm>
#include <vector>

namespace lt::slipstream {

class PeekStream : public kj::InputStream {
   public:
    explicit PeekStream(kj::InputStream& inner);

    KJ_DISALLOW_COPY(PeekStream);
    ~PeekStream() noexcept(false);

    // Start peeking
    void start_recording();

    void cancel_recording();

    void stop_recording_rewind();

    // implements InputStream
    size_t tryRead(void* buffer, size_t minBytes, size_t maxBytes) override;

    void skip(size_t bytes) override;

   private:
    static constexpr size_t size_ = 4096;
    kj::InputStream& inner_;
    bool recording_;
    uint8_t buffer_[size_];
    size_t write_offset_;
    size_t read_offset_;
};

class Scanner : public kj::InputStream {
   // An input stream that can skip to the next Slipstream frame

   public:
    virtual ~Scanner() noexcept(false);

    virtual void reset() = 0;

    virtual bool next() = 0;

    virtual bool peek(uint64_t& source_timestamp) = 0;

    virtual bool peek(uint64_t& source_timestamp, Envelope& envelope) = 0;

    virtual size_t copy_frame(kj::OutputStream& out) = 0;
};

class ScannerWrapper : public Scanner {
    // Implements Scanner in terms of an InputStream.
    //
    // Note that the underlying stream's position is unpredictable once
    // the wrapper is destroyed.

   public:
    explicit ScannerWrapper(kj::InputStream& inner);

    KJ_DISALLOW_COPY(ScannerWrapper);
    virtual ~ScannerWrapper() noexcept(false);

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
    PeekStream inner_;

    static constexpr uint32_t marker_checksum_ =
        (frame_marker[2] << 16) | (frame_marker[1] << 8) | frame_marker[0];
    uint32_t checksum_;

    int buffered_;
    bool at_eof_;
    bool seen_;

    uint64_t source_timestamp_;
    uint64_t envelope_length_;

    uint8_t advance();
    void soft_reset();
};

class PathScanner : public Scanner {
   public:

    PathScanner(const std::string& path);

    PathScanner(PathScanner&& ps);

    KJ_DISALLOW_COPY(PathScanner);
    virtual ~PathScanner() noexcept(false);

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
    std::unique_ptr<kj::FdInputStream> in_;
    std::unique_ptr<ScannerWrapper> scanner_;
};

template <typename T>
class ScannerGroup : public Scanner {
   public:
    ScannerGroup(std::vector<T>& scanners) : scanners_(scanners), sorted_(false)
    {
        for (size_t ix=0; ix < scanners.size(); ++ix) {
            ixs_.push_back(ix);
        }
    }

    KJ_DISALLOW_COPY(ScannerGroup);
    virtual ~ScannerGroup() noexcept(false) {}

    // implements Scanner
    void reset() {
        for (auto&& s : scanners_) {
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
    std::vector<T>& scanners_;
    std::vector<int> ixs_;
    bool sorted_;

    void sort() {
        if (sorted_ || ixs_.empty()) {
            return;
        }

        std::sort(ixs_.begin(), ixs_.end(),
            [&](int i1, int i2) {
                uint64_t t1, t2;

                if (!scanners_[i1].peek(t1)) {
                    return false;
                }
                if (!scanners_[i2].peek(t2)) {
                    return true;
                }
                return t1 < t2;
            });

        sorted_ = true;
    }

    T& head(bool stay_sorted=true) {
        sort();
        sorted_ = stay_sorted;
        return scanners_[ixs_[0]];
    }
};

class PathScannerGroup : public Scanner {
   public:

    PathScannerGroup(const std::vector<std::string>& paths);

    PathScannerGroup(PathScannerGroup&& other);

    KJ_DISALLOW_COPY(PathScannerGroup);
    virtual ~PathScannerGroup() noexcept(false);

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
    std::vector<PathScanner> scanners_;
    std::unique_ptr<ScannerGroup<PathScanner>> scanner_group_;
};

} // namespace lt::slipstream
