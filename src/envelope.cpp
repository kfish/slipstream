#include "lt/slipstream/envelope.h"

#include <capnp/message.h>
#include <capnp/serialize.h>

#include "lt/slipstream/capnp/slipstream.capnp.h"

namespace lt::slipstream {

static capnp::Envelope::PayloadKind fromPayloadKind(PayloadKind payloadKind)
{
    capnp::Envelope::PayloadKind value = capnp::Envelope::PayloadKind::HEADER;

    if (auto data = std::get_if<PayloadData>(&payloadKind)) {
        if (std::get_if<PayloadKeyframe>(data)) {
            value = capnp::Envelope::PayloadKind::KEYFRAME;
        } else {
            value = capnp::Envelope::PayloadKind::DELTA;
        }
    }

    return value;
}

static void envelope_message(::capnp::MallocMessageBuilder& message, const Envelope& envelope)
{
    capnp::Envelope::Builder capnp_envelope = message.initRoot<capnp::Envelope>();

    capnp_envelope.setEncoding(envelope.encoding);
    capnp_envelope.setHostName(envelope.identifier.host_name);
    capnp_envelope.setApplicationName(envelope.identifier.application_name);
    capnp_envelope.setChannelName(envelope.identifier.channel_name);
    capnp_envelope.setPayloadKind(fromPayloadKind(envelope.payload_kind));
}

size_t Envelope::size() const
{
    ::capnp::MallocMessageBuilder message;
    envelope_message(message, *this);
    return ::capnp::computeSerializedSizeInWords(message) * 8;
}

void Envelope::write(kj::OutputStream& out) const
{
    ::capnp::MallocMessageBuilder message;

    envelope_message(message, *this);

    ::capnp::writeMessage(out, message);
}

static PayloadKind toPayloadKind(capnp::Envelope::PayloadKind payloadKind)
{
    PayloadKind value = PayloadKeyframe{};

    switch(payloadKind) {
        case capnp::Envelope::PayloadKind::HEADER:
            value = PayloadHeader{};
            break;
        case capnp::Envelope::PayloadKind::KEYFRAME:
            value = PayloadKeyframe{};
            break;
        case capnp::Envelope::PayloadKind::DELTA:
            value = PayloadDelta{};
            break;
    };

    return value;
}

bool Envelope::read(kj::InputStream& in, size_t length)
{
    try {
        ::capnp::InputStreamMessageReader message(in);

        capnp::Envelope::Reader capnp_envelope = message.getRoot<capnp::Envelope>();

        identifier.host_name = capnp_envelope.getHostName();
        identifier.application_name = capnp_envelope.getApplicationName();
        identifier.channel_name = capnp_envelope.getChannelName();
        encoding = capnp_envelope.getEncoding();
        payload_kind = toPayloadKind(capnp_envelope.getPayloadKind());
    } catch (std::exception&) {
        return false;
    }

    return true;
}

} // namespace lt::slipstream
