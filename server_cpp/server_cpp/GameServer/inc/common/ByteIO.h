#pragma once

#include "common/Types.h"
#include <vector>

struct ByteWriter
{
    ByteBuffer buf;

    void WriteU16LE(uint16 v)
    {
        buf.push_back((Byte)(v & 0xFF));
        buf.push_back((Byte)((v >> 8) & 0xFF));
    }

    void WriteU32LE(uint32 v)
    {
        buf.push_back((Byte)(v & 0xFF));
        buf.push_back((Byte)((v >> 8) & 0xFF));
        buf.push_back((Byte)((v >> 16) & 0xFF));
        buf.push_back((Byte)((v >> 24) & 0xFF));
    }
};

struct ByteReader
{
    const Byte* p;
    size_t len;
    size_t pos{ 0 };

    ByteReader(const Byte* data, size_t size) : p(data), len(size) {}

    bool CanRead(size_t n) const { return pos + n <= len; }

    bool ReadU32LE(uint32& out)
    {
        if (!CanRead(4)) return false;
        out = (uint32)p[pos]
            | ((uint32)p[pos + 1] << 8)
            | ((uint32)p[pos + 2] << 16)
            | ((uint32)p[pos + 3] << 24);
        pos += 4;
        return true;
    }
};

// [uint16 length][uint16 msg_id][payload...], length = 2 + payloadLen
inline ByteBuffer BuildFrame(MsgId msgId, const Byte* payload, size_t payloadLen)
{
    const uint16 length = (uint16)(2 + payloadLen);

    ByteBuffer out;
    out.reserve((size_t)2 + (size_t)length);

    // length LE
    out.push_back((Byte)(length & 0xFF));
    out.push_back((Byte)((length >> 8) & 0xFF));

    // msg_id LE
    out.push_back((Byte)(msgId & 0xFF));
    out.push_back((Byte)((msgId >> 8) & 0xFF));

    // payload
    if (payloadLen > 0)
        out.insert(out.end(), payload, payload + payloadLen);

    return out;
}
