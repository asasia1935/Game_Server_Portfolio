#pragma once
#include "../common/Types.h"
#include <string>

struct Frame
{
    MsgId msgId = 0;
    ByteBuffer payload;
};

enum class FrameError
{
    None,
    LengthTooSmall,     // length < 2 (msg_id 포함 못함)
    FrameTooLarge,      // total(2+length) > MAX_FRAME_TOTAL
    RecvBufferTooLarge  // _buf > MAX_RECV_BUFFER
};

class PacketFramer
{
public:
    PacketFramer() = default;

    // 수신 데이터를 내부 버퍼에 누적
    // 성공: true, 실패: false + lastError 설정
    bool Append(const Byte* data, size_t len);

    // 완성된 프레임 1개를 꺼낸다
    // Ok: outFrame 채워짐
    // NeedMore: 아직 프레임 완성 안됨
    // Error: lastError 설정됨 (세션 disconnect 권장)
    PopResult TryPopFrame(Frame& outFrame);

    void Clear();
    size_t BufferedSize() const { return _buf.size(); }

    FrameError LastError() const { return _lastError; }
    const std::string& LastErrorMessage() const { return _lastErrorMsg; }

private:
    // 리틀엔디안으로 u16 읽기 (buf[off], buf[off+1])
    static uint16 PeekU16LE(const ByteBuffer& buf, size_t off);

    // 버퍼 앞에서 n 바이트 제거
    void Consume(size_t n);

    void SetError(FrameError err, const char* msg);

private:
    ByteBuffer _buf;
    FrameError _lastError = FrameError::None;
    std::string _lastErrorMsg;
};