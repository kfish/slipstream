#include "lt/slipstream/framing.h"

#include <endian.h>

namespace lt::slipstream {

void Framing::encode(uint8_t * buf) const
{
    buf[0] = frame_marker[0];
    buf[1] = frame_marker[1];
    buf[2] = frame_marker[2];
    buf[3] = frame_version;

    uint16_t * buf_checksum = reinterpret_cast<uint16_t*>(&buf[4]);
    *buf_checksum = htobe16(checksum);

    buf[6] = sync ? 0x01 : 0x00;
    buf[7] = frame_header_length;

    uint32_t envelope_length_be = htobe32(envelope_length);
    uint8_t * e = reinterpret_cast<uint8_t*>(&envelope_length_be);

    uint32_t payload_length_be = htobe32(payload_length);
    uint8_t * p = reinterpret_cast<uint8_t*>(&payload_length_be);

    buf[8] = (e[2] << 4) | (e[3] >> 4);
    buf[9] = (e[3] << 4) | (p[1] & 0x0f);

    buf[10] = p[2];
    buf[11] = p[3];

    uint64_t * buf_source_timestamp = reinterpret_cast<uint64_t*>(&buf[12]);
    *buf_source_timestamp = htobe64(source_timestamp);
}

std::pair<bool, ssize_t> Framing::write(kj::OutputStream& out) const
{
    ssize_t total = 0;

    try {
        out.write (frame_marker, sizeof(frame_marker));
        total += sizeof(frame_marker);
    } catch(const std::exception&) {
        return { false, total };
    }

    uint8_t buf[4];

    // Version
    buf[0] = frame_version;

    try {
        out.write (buf, 1);
        total += 1;
    } catch(const std::exception&) {
        return { false, total };
    }

    // Checksum
    uint16_t checksum_be = htobe16(checksum);

    try {
        out.write(&checksum_be, sizeof(uint16_t));
        total += sizeof(uint16_t);
    } catch(const std::exception&) {
        return { false, total };
    }

    // Flags, header_len
    if (sync) {
        buf[0] = 0x01;
    } else {
        buf[0] = 0x00;
    }

    buf[1] = frame_header_length;

    try {
        out.write (buf, 2);
        total += 2;
    } catch(const std::exception&) {
        return { false, total };
    }

    uint32_t envelope_length_be = htobe32(envelope_length);
    uint8_t * e = reinterpret_cast<uint8_t*>(&envelope_length_be);

    uint32_t payload_length_be = htobe32(payload_length);
    uint8_t * p = reinterpret_cast<uint8_t*>(&payload_length_be);

    buf[0] = (e[2] << 4) | (e[3] >> 4);
    buf[1] = (e[3] << 4) | (p[1] & 0x0f);

    buf[2] = p[2];
    buf[3] = p[3];

    try {
        out.write (buf, 4);
        total += 4;
    } catch(const std::exception&) {
        return { false, total };
    }

    uint64_t source_timestamp_be = htobe64(source_timestamp);

    try {
        out.write(&source_timestamp_be, sizeof(uint64_t));
        total += sizeof(uint64_t);
    } catch(const std::exception&) {
        return { false, total };
    }

    return {true, total};
}

bool Framing::decode(const uint8_t * buf)
{
    if (buf[0] != frame_marker[0] ||
        buf[1] != frame_marker[1] ||
        buf[2] != frame_marker[2] ||
        buf[3] != frame_version ||
        buf[7] != frame_header_length) {
        return false;
    }

    const uint16_t * buf_checksum =
        reinterpret_cast<const uint16_t*>(&buf[4]);
    checksum = be16toh(*buf_checksum);

    if (buf[6] == 0x00) {
        sync = false;
    } else if (buf[6] == 0x01) {
        sync = true;
    } else {
        return false;
    }

    uint32_t envelope_length_be = 0;
    uint8_t * e = reinterpret_cast<uint8_t*>(&envelope_length_be);

    uint32_t payload_length_be = 0;
    uint8_t * p = reinterpret_cast<uint8_t*>(&payload_length_be);

    e[2] = (buf[8] & 0xf0) >> 4;
    e[3] = ((buf[8] & 0x0f) << 4) | ((buf[9] & 0xf0) >> 4);

    envelope_length = be32toh(envelope_length_be);

    p[1] = buf[9] & 0x0f;
    p[2] = buf[10];
    p[3] = buf[11];

    payload_length = be32toh(payload_length_be);;

    const uint64_t * buf_source_timestamp =
        reinterpret_cast<const uint64_t*>(&buf[12]);
    source_timestamp = be64toh(*buf_source_timestamp);

    return true;
}

std::pair<bool, ssize_t> Framing::read(kj::InputStream& in)
{
    ssize_t total = 0;
    uint8_t buf[4];

    try {
        in.read(buf, 4);
        total += 4;
    } catch(const std::exception&) {
        return { false, total };
    }

    if (buf[0] != frame_marker[0] ||
        buf[1] != frame_marker[1] ||
        buf[2] != frame_marker[2] ||
        buf[3] != frame_version) {
        return { false, total };
    }

    uint16_t checksum_be = 0;

    try {
        in.read(&checksum_be, sizeof(uint16_t));
        total += sizeof(uint16_t);
    } catch(const std::exception&) {
        return { false, total };
    }

    checksum = be16toh(checksum_be);

    try {
        in.read(buf, 2);
        total += 2;
    } catch(const std::exception&) {
        return { false, total };
    }

    if (buf[0] == 0x00) {
        sync = false;
    } else if (buf[0] == 0x01) {
        sync = true;
    } else {
        return { false, total };
    }

    if (buf[1] != frame_header_length) {
        return { false, total };
    }

    try {
        in.read(buf, 4);
        total += 4;
    } catch(const std::exception&) {
        return { false, total };
    }

    uint32_t envelope_length_be = 0;
    uint8_t * e = reinterpret_cast<uint8_t*>(&envelope_length_be);

    uint32_t payload_length_be = 0;
    uint8_t * p = reinterpret_cast<uint8_t*>(&payload_length_be);

    e[2] = (buf[0] & 0xf0) >> 4;
    e[3] = ((buf[0] & 0x0f) << 4) | ((buf[1] & 0xf0) >> 4);

    envelope_length = be32toh(envelope_length_be);

    p[1] = buf[1] & 0x0f;
    p[2] = buf[2];
    p[3] = buf[3];

    payload_length = be32toh(payload_length_be);;

    uint64_t source_timestamp_be = 0;

    try {
        in.read(&source_timestamp_be, sizeof(uint64_t));
        total += sizeof(uint64_t);
    } catch(const std::exception&) {
        return { false, total };
    }

    source_timestamp = be64toh(source_timestamp_be);

    return { true, total };
}


} // namespace lt::slipstream
