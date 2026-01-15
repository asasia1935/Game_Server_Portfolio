#include "net/Session.h"
#include "common/ByteIO.h"

#include <iostream>

static constexpr MsgId C_Ping = 1101;
static constexpr MsgId S_Pong = 1102;

static void Log(const std::string& tag, const std::string& msg)
{
    std::cout << "[" << tag << "] " << msg << "\n";
}

Session::Session(SOCKET sock) : _sock(sock)
{
    _tag = "Session";
}

Session::~Session()
{
    Stop();
}

void Session::Start()
{
    if (_running.exchange(true)) return;
    _thread = std::thread(&Session::RecvLoop, this);
}

void Session::Stop()
{
    // 언제든 정지 요청 (이미 false여도 상관 없음)
    _running.store(false);

    // recv를 깨우기 위해 소켓 닫기
    CloseSocket();

    // join은 running 상태랑 무관하게 해야 함
    if (_thread.joinable())
    {
        // 혹시 스레드 내부에서 Stop이 호출되는 경우를 대비
        if (std::this_thread::get_id() != _thread.get_id())
            _thread.join();
    }
}

void Session::RecvLoop()
{
    Log(_tag, "RecvLoop started");

    Byte temp[4096];

    while (_running.load())
    {
        int n = ::recv(_sock, (char*)temp, (int)sizeof(temp), 0);
        if (n > 0)
        {
            OnRecv(temp, (size_t)n);
        }
        else
        {
            // n == 0: 정상 종료, n < 0: 에러
            break;
        }
    }

    _running.store(false);
    CloseSocket();
    Log(_tag, "RecvLoop ended");
}

void Session::OnRecv(const Byte* data, size_t len)
{
    if (!_framer.Append(data, len))
    {
        Log(_tag, std::string("Framer Append error: ") + _framer.LastErrorMessage());
        Stop();
        return;
    }

    while (_running.load())
    {
        Frame frame;
        auto r = _framer.TryPopFrame(frame);

        if (r == PopResult::Ok)
        {
            Dispatch(frame);
        }
        else if (r == PopResult::NeedMore)
        {
            break;
        }
        else
        {
            Log(_tag, std::string("Framer pop error: ") + _framer.LastErrorMessage());
            Stop();
            break;
        }
    }
}

void Session::Dispatch(const Frame& frame)
{
    if (frame.msgId == C_Ping)
    {
        // payload = u32 seq
        ByteReader br(frame.payload.data(), frame.payload.size());
        uint32 seq = 0;
        if (!br.ReadU32LE(seq))
        {
            Log(_tag, "C_Ping malformed payload (need u32)");
            Stop();
            return;
        }

        // reply: S_Pong(seq)
        ByteWriter w;
        w.WriteU32LE(seq);
        SendFrame(S_Pong, w.buf.data(), w.buf.size());
        return;
    }

    // Tier1 정책: 모르는 msg -> disconnect
    Log(_tag, "Unknown msgId=" + std::to_string(frame.msgId) + " -> disconnect");
    Stop();
}

bool Session::SendFrame(MsgId msgId, const Byte* payload, size_t payloadLen)
{
    std::lock_guard<std::mutex> lock(_sendMutex);

    ByteBuffer frame = BuildFrame(msgId, payload, payloadLen);
    return SendAll(frame.data(), frame.size());
}

bool Session::SendAll(const Byte* data, size_t len)
{
    size_t sent = 0;
    while (sent < len && _running.load())
    {
        int n = ::send(_sock, (const char*)(data + sent), (int)(len - sent), 0);
        if (n <= 0)
            return false;

        sent += (size_t)n;
    }
    return sent == len;
}

void Session::CloseSocket()
{
    if (_sock == INVALID_SOCKET) return;

    ::shutdown(_sock, SD_BOTH);
    ::closesocket(_sock);
    _sock = INVALID_SOCKET;
}
