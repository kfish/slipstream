#pragma once

#include <kj/io.h>

#include "lt/slipstream/types.h"

namespace lt::slipstream {

template <typename T>
class Serialize {
   public:
    size_t size() const
    {
        return T::size_impl(*(static_cast<const T*>(this)));
    }

    bool read(kj::InputStream& in, size_t length)
    {
        return T::read_impl(in, *(static_cast<T*>(this)), length);
    }

    bool write(kj::OutputStream& out) const
    {
        return T::write_impl(out, *(static_cast<const T*>(this)));
    }
};

template <typename T>
class Headerless {
   public:
    using header_type = no_type;
    using data_type = typename T::data_type;
    using delta_type = no_type;

    static bool has_header_encoding(const std::string& encoding)
    {
        return false;
    }

    static bool has_encoding(const std::string& encoding)
    {
        return encoding == T::encoding;
    }

    static bool has_delta_encoding(const std::string& encoding)
    {
        return false;
    }

    static const std::string encoding(const data_type& value)
    {
        return T::encoding;
    }

    size_t size(const data_type& value)
    {
        return T::size_impl(value);
    }

    bool write(kj::OutputStream& out, const data_type& value)
    {
        return T::write_impl(out, value);
    }

    static bool read_header(kj::InputStream& in, no_type& header)
    {
        return false;
    }

    bool read(kj::InputStream& in, data_type& value, size_t length)
    {
        return T::read_impl(in, value, length);
    }

    const std::string to_json(const data_type& value)
    {
        return T::to_json(value);
    }

   private:
    T thang_;
};

template <typename T>
class HeaderStream {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;
    using delta_type = no_type;

    HeaderStream(const header_type& header) : thang_(header)
    {
    }

    static bool has_header_encoding(const std::string& encoding)
    {
        return encoding == header_type::encoding;
    }

    static bool has_encoding(const std::string& encoding)
    {
        return encoding == data_type::encoding;
    }

    static bool has_delta_encoding(const std::string& encoding)
    {
        return false;
    }

    static const std::string header_encoding(const header_type& value)
    {
        return header_type::encoding;
    }

    static const std::string encoding(const data_type& value)
    {
        return data_type::encoding;
    }

    size_t size_header()
    {
        return thang_.header().size();
    }

    bool write_header (kj::OutputStream& out)
    {
        return thang_.header().write(out);
    }

    size_t size(const data_type& i)
    {
        return thang_.encode(i).size();
    }

    bool write(kj::OutputStream& out, const data_type& i)
    {
        return thang_.encode(i).write(out);
    }

    static bool read_header(kj::InputStream& in, header_type& header, size_t length)
    {
        return header.read(in, length);
    }

    bool read(kj::InputStream& in, data_type& i, size_t length)
    {
        data_type v;

        if (v.read(in, length)) {
            i = thang_.decode(v);
            return true;
        }

        return false;
    }

    const header_type& header() {
        return thang_.header();
    }

   private:
    T thang_;
};

template <typename T>
class HeaderDeltaStream {
   public:
    using header_type = typename T::header_type;
    using data_type = typename T::data_type;
    using delta_type = typename T::delta_type;

    HeaderDeltaStream(const header_type& header) : thang_(header)
    {
    }

    static bool has_header_encoding(const std::string& encoding)
    {
        return encoding == header_type::encoding;
    }

    static bool has_encoding(const std::string& encoding)
    {
        return encoding == data_type::encoding;
    }

    static bool has_delta_encoding(const std::string& encoding)
    {
        return encoding == delta_type::encoding;
    }

    static const std::string header_encoding(const header_type& value)
    {
        return header_type::encoding;
    }

    static const std::string encoding(const data_type& value)
    {
        return data_type::encoding;
    }

    static const std::string delta_encoding(const data_type& value)
    {
        return delta_type::encoding;
    }

    size_t size_header()
    {
        return thang_.header().size();
    }

    bool write_header (kj::OutputStream& out)
    {
        return thang_.header().write(out);
    }

    size_t size(const data_type& i)
    {
        return thang_.encode(i).size();
    }

    bool write(kj::OutputStream& out, const data_type& i)
    {
        return thang_.encode(i).write(out);
    }

    size_t size_delta(const data_type& i)
    {
        return thang_.encode_delta(i).size();
    }

    bool write_delta(kj::OutputStream& out, const delta_type& i)
    {
        return thang_.encode_delta(i).write(out);
    }

    static bool read_header(kj::InputStream& in, header_type& header, size_t length)
    {
        return header.read(in, length);
    }

    bool read(kj::InputStream& in, data_type& i, size_t length)
    {
        data_type v;

        if (v.read(in, length)) {
            i = thang_.decode(v);
            return true;
        }

        return false;
    }

    bool read_delta(kj::InputStream& in, data_type& i, size_t length)
    {
        delta_type v;

        if (v.read(in, length)) {
            i = thang_.decode_delta(v);
            return true;
        }

        return false;
    }

    const header_type& header() {
        return thang_.header();
    }

   private:
    T thang_;
};

} // namespace lt::slipstream
