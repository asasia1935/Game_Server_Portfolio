#pragma once

#include "common/Types.h"
#include "net/PacketFramer.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <string>

class Session
{
public:
    using SessionId = uint64_t;
    using OnCloseFn = std::function<void(SessionId)>;

public:
    explicit Session(SOCKET sock, SessionId id, OnCloseFn onClose);
	~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void Start();
	void Stop();                // 외부 전용 (join 함)
	void RequestStop();         // 내부/외부 어디서든 호출 (join 안함)

    SessionId Id() const { return _id; }

    bool IsRunning() const { return _running.load(); }
    
	// SendFrame: 큐에 넣는 작업만
    bool SendFrame(MsgId msgId, const Byte* payload, size_t payloadLen);

private:
    void RecvLoop();
	void SendLoop(); // send queue flush 용도

    void OnRecv(const Byte* data, size_t len);
    void Dispatch(const Frame& frame);

    bool SendAll(const Byte* data, size_t len);
    void CloseSocket();

private:
    SessionId _id{ 0 };
    OnCloseFn _onClose;

    SOCKET _sock{ INVALID_SOCKET };
    std::atomic<bool> _running{ false };

    std::thread _recvThread;
    std::thread _sendThread;

    PacketFramer _framer;

	// Send queue 관련
    std::mutex _sendMutex;
    std::condition_variable _sendCv;
    std::deque<ByteBuffer> _sendQ;

    std::string _tag;
};
