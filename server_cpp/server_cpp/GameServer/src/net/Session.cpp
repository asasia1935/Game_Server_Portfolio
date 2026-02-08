#include "net/Session.h"
#include "common/ByteIO.h"

#include <iostream>

static constexpr MsgId C_Ping = 1101;
static constexpr MsgId S_Pong = 1102;

static void Log(const std::string& tag, const std::string& msg)
{
    std::cout << "[" << tag << "] " << msg << "\n";
}

Session::Session(SOCKET sock, SessionId id, OnCloseFn onClose) : _sock(sock), _onClose(std::move(onClose)), _id(id)
{
    _tag = "Session #" + std::to_string(_id);
}

void Session::Start()
{
    if (_running.exchange(true)) return;

    _recvThread = std::thread(&Session::RecvLoop, this);
    _sendThread = std::thread(&Session::SendLoop, this);
}

void Session::Stop()
{
    RequestStop();

    // 외부에서 호출하면 스레드 join
    if (_recvThread.joinable())
        _recvThread.join();
    if (_sendThread.joinable())
        _sendThread.join();
}

void Session::RequestStop()
{
    // 내부/외부 어디서든 호출 가능 (멱등)
    _running.store(false, std::memory_order_relaxed);

    // send thread 깨우기 -> 대기중인 send 스레드 즉시 종료 (wait에서 깨어나서 조건 확인 후 큐가 비면 break -> 스레드 종료)
    _sendCv.notify_all();

    // recv를 깨우기 위해 소켓 닫기(멱등해야 함)
    CloseSocket();
}

bool Session::SendFrame(MsgId msgId, const Byte* payload, size_t payloadLen)
{
    if (!_running.load(std::memory_order_relaxed))
        return false;

    ByteBuffer frame = BuildFrame(msgId, payload, payloadLen);

    {
        std::lock_guard<std::mutex> lock(_sendMutex);
        _sendQ.emplace_back(std::move(frame));
    }

    _sendCv.notify_one();
    return true;
}

void Session::RecvLoop()
{
    Log(_tag, "RecvLoop started");

    Byte temp[4096];

    while (_running.load(std::memory_order_relaxed))
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

    // 종료 정리
    _running.store(false, std::memory_order_relaxed);
    _sendCv.notify_all(); // send loop도 빠져나가게

    CloseSocket();

    Log(_tag, "RecvLoop ended");

    // 세션 종료 알림
    if (_onClose)
        _onClose(_id);
}

void Session::SendLoop()
{
    Log(_tag, "SendLoop started");

    for (;;)
    {
        ByteBuffer toSend;

        {
            std::unique_lock<std::mutex> lock(_sendMutex);

            // 큐가 비었고 아직 running이면 대기
            _sendCv.wait(lock, [&] {
                return !_sendQ.empty() || !_running.load(std::memory_order_relaxed);
                });

            // running=false이고 보낼 것도 없으면 종료
            if (_sendQ.empty() && !_running.load(std::memory_order_relaxed))
                break;

            // 하나 꺼내기
            if (!_sendQ.empty())
            {
                toSend = std::move(_sendQ.front());
                _sendQ.pop_front();
            }
        }

        if (!toSend.empty())
        {
            // 실제 send는 send thread 단독으로 수행 => 프레임 섞임 없음
            if (!SendAll(toSend.data(), toSend.size()))
            {
                // send 실패면 종료 요청
                RequestStop();
                break;
            }
        }
    }

    Log(_tag, "SendLoop ended");
}

void Session::OnRecv(const Byte* data, size_t len)
{
    if (!_framer.Append(data, len))
    {
        Log(_tag, std::string("Framer Append error: ") + _framer.LastErrorMessage());
        RequestStop();
        return;
    }

    while (_running.load(std::memory_order_relaxed))
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
            RequestStop();
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
            RequestStop();
            return;
        }

        Log(_tag, "C_Ping Received!");

        // reply: S_Pong(seq)
        ByteWriter w;
        w.WriteU32LE(seq);
        SendFrame(S_Pong, w.buf.data(), w.buf.size());

        Log(_tag, "S_Pong Sent!");
        return;
    }

    // Tier1 정책: 모르는 msg -> disconnect
    Log(_tag, "Unknown msgId=" + std::to_string(frame.msgId) + " -> disconnect");
    RequestStop();
}

bool Session::SendAll(const Byte* data, size_t len)
{
    size_t sent = 0;
    while (sent < len && _running.load(std::memory_order_relaxed))
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

    _sock = INVALID_SOCKET;

    ::shutdown(_sock, SD_BOTH);
    ::closesocket(_sock);
}
