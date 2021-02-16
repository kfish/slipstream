#pragma once

#include <capnp/compat/json.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/io.h>
#include <string>

#include "lt/slipstream/serialize.h"

namespace lt::slipstream {

template <typename T, typename CapnpType, typename RawType>
class SlipstreamCapnp : public Serialize<T> {
   protected:
    using reader_type = typename CapnpType::Reader;
    using builder_type = typename CapnpType::Builder;

   public:
    using data_type = RawType;

    static const data_type decode(const reader_type& reader)
    {
        return T::decode(reader);
    }

    static void encode(builder_type& builder, const data_type& value)
    {
        T::encode(builder, value);
    }

    static size_t size_impl(const data_type& value)
    {
        ::capnp::MallocMessageBuilder message;

        builder_type capnp_builder = message.initRoot<CapnpType>();
        encode(capnp_builder, value);

        return ::capnp::computeSerializedSizeInWords(message) * 8;
    }

    static bool read_impl(kj::InputStream& in, data_type& value, size_t length)
    {
        ::capnp::InputStreamMessageReader message(in);
        reader_type capnp_reader = message.getRoot<CapnpType>();
        value = decode(capnp_reader);

        return true;
    }

    static const std::string read_json(kj::InputStream& in)
    {
        ::capnp::JsonCodec json;

        ::capnp::InputStreamMessageReader message(in);
        reader_type capnp_reader = message.getRoot<CapnpType>();
        return json.encode(capnp_reader).cStr();
    }

    static const std::string to_json(const data_type& value)
    {
        ::capnp::MallocMessageBuilder message;

        builder_type capnp_builder = message.initRoot<CapnpType>();
        encode(capnp_builder, value);

        ::capnp::JsonCodec json;
        return json.encode(capnp_builder).cStr();
    }

    static bool write_impl(kj::OutputStream& out, const data_type& value)
    {
        ::capnp::MallocMessageBuilder message;

        builder_type capnp_builder = message.initRoot<CapnpType>();
        encode(capnp_builder, value);

        ::capnp::writeMessage(out, message);

        return true;
    }
};


} // namespace lt::slipstream
