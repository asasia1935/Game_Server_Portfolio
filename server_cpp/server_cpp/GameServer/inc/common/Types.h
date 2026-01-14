#pragma once

#include <cstdint>
#include <vector>

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using Byte = uint8;
using ByteBuffer = std::vector<Byte>;

using MsgId = uint16;

constexpr size_t MAX_FRAME_TOTAL = 4096;
constexpr size_t MAX_RECV_BUFFER = 64 * 1024;

enum class PopResult
{
    Ok,
    NeedMore,
    Error
};