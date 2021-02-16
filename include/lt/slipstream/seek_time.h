#pragma once

/*
 * This file exists due to the follwoing broken diamond dependency:
 *
 *
 *                 kj::InputStream
 *                       / \
 *                      X   \
 *                           \
 *        SeekableStream    Scanner
 *                     \     /
 *                      \   /
 *                      Seeker
 *
 * In order to implement instances of Seeker, we need to use the methods
 * introduced by both kj::InputStream (tryRead, skip) and SeekableStream
 * (seek, tell).
 *
 * The template seek_time_generic<T> implements this for a given type T,
 * if T derives from both kj::InputStream and SeekableStream.
 */

#include <limits>

namespace lt::slipstream {

template <typename T>
bool tell_time(T& t, uint64_t& offset, uint64_t& timestamp)
{
    if (!t.peek(timestamp)) {
        try {
            if (t.next()) {
                t.peek(timestamp);
            } else {
                return false;
            }
        } catch (const std::exception&) {
            return false;
        }
    }
    int64_t o = t.tell();
    if (o >= 0) {
        offset = o;
    }
    return true;
}

class SeekBounds {
   public:
    SeekBounds()
    {}

    bool contains(uint64_t target_timestamp)
    {
        return (target_timestamp >= lower_timestamp_ && target_timestamp <= upper_timestamp_);
    }

    template <typename T>
    void init(T& in) {
        uint64_t cur = in.tell();

        in.seek(0, SEEK_SET);
        in.next();
        tell_time<T>(in, lower_offset_, lower_timestamp_);

        in.seek(-4096, SEEK_END);

        roll<T>(in, std::numeric_limits<uint64_t>::max());

        tell_time<T>(in, upper_offset_, upper_timestamp_);

        // Reset offset
        in.seek(cur, SEEK_SET);
    }

    template <typename T>
    bool refine(T& in, uint64_t target_timestamp) {
        uint64_t timestamp = 0;
        uint64_t offset = 0;
        tell_time<T>(in, offset, timestamp);

        if (offset > upper_offset_) {
            upper_offset_ = offset;
            upper_timestamp_ = timestamp;
        } else if (offset < lower_offset_) {
            lower_offset_ = offset;
            lower_timestamp_ = timestamp;
        } else if (timestamp == target_timestamp) {
            return false;
        } else if (timestamp < target_timestamp) {
            if (offset == lower_offset_) {
                return false;
            }

            lower_offset_ = offset;
            lower_timestamp_ = timestamp;
        } else {
            if (offset == upper_offset_) {
                // Reset to lower before rolling forward
                in.seek(lower_offset_, SEEK_SET);
                return false;
            }

            upper_offset_ = offset;
            upper_timestamp_ = timestamp;
        }

        return true;
    }

    template <typename T>
    bool roll(T& in, uint64_t target_timestamp) {
        uint64_t prev_timestamp = 0;
        uint64_t prev_offset = 0;
        uint64_t timestamp = 0;
        uint64_t offset = 0;

        tell_time<T>(in, prev_offset, prev_timestamp);

        if (prev_timestamp > target_timestamp) {
            return false;
        }

        while (true) {
            try {
                if (!in.next()) {
                    break;
                }
            } catch (std::exception&) {
                break;
            }
            tell_time<T>(in, offset, timestamp);
            if (timestamp > target_timestamp) {
                break;
            }
            prev_timestamp = timestamp;
            prev_offset = offset;
        }

        in.seek(prev_offset, SEEK_SET);

        return true;
    }

    template <typename T>
    bool step(T& in, uint64_t target_timestamp) {
        if (!contains(target_timestamp)) {
            return false;
        }

        if (upper_offset_ - lower_offset_ < step_size_) {
            in.seek(lower_offset_, SEEK_SET);
            return roll<T>(in, target_timestamp);
            // Throw an exception if roll returns false?
        } else {
            uint64_t numerator = (target_timestamp - lower_timestamp_) * (upper_offset_ - lower_offset_);
            uint64_t denominator = upper_timestamp_ - lower_timestamp_;
            uint64_t target_offset = lower_offset_ + (numerator / denominator);
            try {
                in.seek(target_offset, SEEK_SET);
            } catch (std::exception&) {
                return false;
            }
            return true;
        }
    }

   private:
    static constexpr uint64_t step_size_ = 4096;
    uint64_t lower_timestamp_ = 0;
    uint64_t upper_timestamp_ = std::numeric_limits<uint64_t>::max();
    uint64_t lower_offset_ = 0;
    uint64_t upper_offset_ = std::numeric_limits<uint64_t>::max();
};

template <class T>
bool seek_time_bisect(T& t, uint64_t target_timestamp)
{
    SeekBounds bounds;

    bounds.init<T>(t);

    if (!bounds.contains(target_timestamp)) {
        return false;
    }

    while (bounds.step<T>(t, target_timestamp)) {
        if (!bounds.refine<T>(t, target_timestamp)) {
            bounds.roll<T>(t, target_timestamp);
            break;
        }
    }

    return true;
}

} // namespace lt::slipstream
