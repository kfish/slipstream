#pragma once

#include <stdint.h>
#include <unistd.h>
#include <utility>

#include <kj/io.h>

namespace lt::slipstream {

static constexpr uint8_t frame_version = 2;
static constexpr size_t frame_header_length = 20;

// 0xff 0xfe is an invliad UTF-8 sequence
static constexpr uint8_t frame_marker[] = { 0xff, 0xfe, 0xed };

// Checksum: Algorithm as for IPv4. Set checksum field to zero, then
// calculate over entire frame (frame header, envelope and payload)
// and set that value in the checksum field.
// To verify, use same algorithm and this field should get reset to
// zero.
//
// Flags: SYNC 0x1

/*
     0       4       8      12      16      20      24      28
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 00 | Marker: 0xff   0xfe            0xed           | Version       |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 32 | Checksum                      | Flags         | Frame hdr len |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 64 | Envelope len (12 bits)| Payload length (20 bits, big endian)  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 96 | Source timestamp (64 bits, big endian)                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
128 | ...                                                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

struct Framing {
    uint32_t envelope_length;
    uint32_t payload_length;
    uint64_t source_timestamp;
    uint16_t checksum;
    bool sync;

    bool operator==(const Framing& other) const
    {
        return
            envelope_length == other.envelope_length &&
            payload_length == other.payload_length &&
            source_timestamp == other.source_timestamp &&
            checksum == other.checksum &&
            sync == other.sync;
    }

    size_t size() const { return frame_header_length; }

    void encode(uint8_t * buf) const;

    bool decode(const uint8_t * buf);

    std::pair<bool, ssize_t> write(kj::OutputStream& out) const;

    std::pair<bool, ssize_t> read(kj::InputStream& in);

};

} // namespace lt::slipstream
