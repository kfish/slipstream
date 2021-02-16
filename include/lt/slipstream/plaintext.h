#pragma once

#include "lt/slipstream/serialize.h"
#include "lt/slipstream/types.h"

#include <endian.h>
#include <optional>
#include <ostream>
#include <string>
#include <unistd.h>

#include <kj/io.h>

static std::string json_escape(const std::string& input)
{
    std::ostringstream ss;

    for (auto&& c : input) {
        switch (c) {
            case '\\': ss << "\\\\"; break;
            case '"': ss << "\\\""; break;
            case '/': ss << "\\/"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << c; break;
        }
    }
    return ss.str();
}

namespace lt::slipstream {

class SerialString : public Serialize<SerialString> {
   public:
    using data_type = SerialString;

    static constexpr auto encoding = "text/plain";

    SerialString(std::string str="") : str_(str) {}

    const std::string& str() const {
        return str_;
    }

    bool operator==(const SerialString& other) const {
        return str_ == other.str_;
    }

    friend std::ostream& operator<<(std::ostream& os, const SerialString& s) {
        return os << s.str_;
    }

    static size_t size_impl(const SerialString& ss)
    {
        return ss.str_.size();
    }

    static bool read_impl(kj::InputStream& in, SerialString& ss, size_t length)
    {
        std::string& s = ss.str_;

        try {
            s.resize(length);
            in.read(&s[0], length);
        } catch(const std::exception&) {
            return false;
        }

        return true;
    }

    static const std::string to_json(const SerialString& ss)
    {
        const std::string& s = ss.str_;
        return "{\"text\":\"" + json_escape(s) + "\"}";
    }

    static bool write_impl(kj::OutputStream& out, const SerialString& ss)
    {
        const std::string& s = ss.str_;

        try{
            out.write(s.c_str(), s.length());
        } catch(const std::exception&) {
            return false;
        }

        return true;
    }

   private:
    std::string str_;
};

using PlainText = Headerless<SerialString>;

} // namespace lt::slipstream
