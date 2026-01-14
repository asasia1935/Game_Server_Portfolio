#include "net/PacketFramer.h"
#include <algorithm>

bool PacketFramer::Append(const Byte* data, size_t len)
{
    if (len == 0) return true;

    // overflow / abuse 방지: push 전 사이즈 체크
    if (_buf.size() + len > MAX_RECV_BUFFER)
    {
        SetError(FrameError::RecvBufferTooLarge, "recv buffer exceeded MAX_RECV_BUFFER");
        return false;
    }

    _buf.insert(_buf.end(), data, data + len);
    return true;
}

PopResult PacketFramer::TryPopFrame(Frame& outFrame)
{
    outFrame = Frame{}; // reset

    // 최소 length(2바이트) 없으면 더 필요
    if (_buf.size() < 2)
        return PopResult::NeedMore;

    const uint16 length = PeekU16LE(_buf, 0); // msg_id(2) + payload

    if (length < 2)
    {
        SetError(FrameError::LengthTooSmall, "frame length < 2 (must include msg_id)");
        return PopResult::Error;
    }

    const size_t total = 2u + static_cast<size_t>(length); // length field 포함 전체
    if (total > MAX_FRAME_TOTAL)
    {
        SetError(FrameError::FrameTooLarge, "frame total size exceeded MAX_FRAME_TOTAL");
        return PopResult::Error;
    }

    if (_buf.size() < total)
        return PopResult::NeedMore;

    // msg_id 읽기 (length 뒤 2바이트)
    const MsgId msgId = PeekU16LE(_buf, 2);
    const size_t payloadLen = static_cast<size_t>(length) - 2u;

    outFrame.msgId = msgId;
    outFrame.payload.resize(payloadLen);

    if (payloadLen > 0)
    {
        // payload 시작 오프셋: 2(length) + 2(msg_id) = 4
        std::copy(_buf.begin() + 4, _buf.begin() + 4 + payloadLen, outFrame.payload.begin());
    }

    Consume(total);
    return PopResult::Ok;
}

void PacketFramer::Clear()
{
    _buf.clear();
    _lastError = FrameError::None;
    _lastErrorMsg.clear();
}

uint16 PacketFramer::PeekU16LE(const ByteBuffer& buf, size_t off)
{
    // 호출부에서 충분한 길이 확인을 보장한다고 가정
    const uint16 lo = static_cast<uint16>(buf[off]);
    const uint16 hi = static_cast<uint16>(buf[off + 1]);
    return static_cast<uint16>(lo | (hi << 8));
}

void PacketFramer::Consume(size_t n)
{
    if (n == 0) return;
    if (n >= _buf.size())
    {
        _buf.clear();
        return;
    }
    _buf.erase(_buf.begin(), _buf.begin() + static_cast<std::ptrdiff_t>(n));
}

void PacketFramer::SetError(FrameError err, const char* msg)
{
    _lastError = err;
    _lastErrorMsg = msg ? msg : "";
}