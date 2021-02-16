@0xf2cdbe561993c8c6;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("lt::slipstream::capnp");

using Nanoseconds = UInt64;
using UnixTimeNs = UInt64;
using ApplicationId = UInt8;

struct Envelope {
  encoding @0 :Text;

  hostName @1 :Text;

  applicationName @2 :Text;

  channelName @3 :Text;

  payloadKind @4 : PayloadKind;

  enum PayloadKind {
    header @0;
    keyframe @1;
    delta @2;
  }
}
