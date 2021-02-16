# slipstream

[![Slipstream 1989 Full Movie](https://img.youtube.com/vi/geCBg8-D5rA/0.jpg)](https://www.youtube.com/watch?v=geCBg8-D5rA "Slipstream 1989 Full Movie")

## Motivation

currently we have different set of loggers and each different logger needs a
different writer/reader implementation. It would be nice to have a generic fast memory
logger. for which reader can normalize the data using the in-place schema file, decode
the data and also apply some filters over it.

## Framing layer (raw binary encoded)

 * Binary marker
 * Frame length (including slipstream headers), or separate envelope/payload lengths?
 * source timestamp (or is this in envelope?)

## Envelope (as capn'proto)
 * host name
 * app name
 * channel (topic, within app namespace)
 * flags: header, keyframe, delta
 * encoding (mime-type, string)

### Tooling

Merge streams from multiple sources (via zeromq etc)
Offline (cmdline) zip/unzip (can roundtrip)

## Payload layer

Header: CapnProto schema encoding (string)

### Tooling

Generic stream -> json websockets thing

## Policy

When streaming, repeat headers every 5 min or so (configurable, perhaps handle within library not app)

## Library

### writer apis

 * apply apis to wrap the payload with an envelope. Envelope generation could
   be initialized using a configuration.
 * stream api to put the envelope + payload to some stream (could be file,
   networks etc.)
 * api to publish the schema file to the same stream as well.

### reader apis

 * offline mode read a dumped file and normalize the result in json or capnproto
   buffer.
 * online mode continuos reading from file/socket.

## implementation

 * separate thread ?
 * configuration using a config file + env variables ?
 * latency requirement on the fast path ?

