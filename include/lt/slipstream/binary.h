#pragma once

#include "lt/slipstream/serialize.h"
#include "lt/slipstream/types.h"

#include <endian.h>
#include <ostream>
#include <string.h>
#include <unistd.h>

#include <kj/io.h>

namespace lt::slipstream {

class SerialBinary : public Serialize<SerialBinary> {
   public:
    using data_type = SerialBinary;

    static constexpr auto encoding = "application/octet-stream";

    SerialBinary() : data_() {}

    SerialBinary(std::vector<uint8_t> data) : data_(data) {}

    SerialBinary(uint8_t * data, size_t length) : data_(data, data+length) {}

    SerialBinary(std::initializer_list<uint8_t> l) : data_(l) {}

    const std::vector<uint8_t> data() const {
        return data_;
    }

    bool operator==(const SerialBinary& other) const {
        return data_ == other.data_;
    }

    friend std::ostream& operator<<(std::ostream& os, const SerialBinary& binary) {
        return os.write(reinterpret_cast<const char *>(&binary.data_[0]), binary.data_.size());
    }

    static size_t size_impl(const SerialBinary& binary)
    {
        return binary.data_.size();
    }

    static bool read_impl(kj::InputStream& in, SerialBinary& binary, size_t length)
    {
        try {
            binary.data_.resize(length);
            in.read(binary.data_.data(), length);
        } catch(const std::exception&) {
            return false;
        }

        return true;
    }

    static const std::string to_json(const SerialBinary& binary)
    {
        // TODO: encode binary data as base64 ?
        return "{\"length\":" + std::to_string(binary.data_.size()) + "}";
    }

    static bool write_impl(kj::OutputStream& out, const SerialBinary& binary)
    {
        try{
            out.write(binary.data_.data(), binary.data_.size());
        } catch(const std::exception&) {
            return false;
        }

        return true;
    }

   private:
    std::vector<uint8_t> data_;
};

using Binary = Headerless<SerialBinary>;

} // namespace lt::slipstream
