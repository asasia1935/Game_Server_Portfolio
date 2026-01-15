#pragma once

#include "common/Types.h"
#include "net/PacketFramer.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <string>

class Session
{
public:
    explicit Session(SOCKET sock);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void Start();
    void Stop();

    bool IsRunning() const { return _running.load(); }

private:
    void RecvLoop();
    void OnRecv(const Byte* data, size_t len);
    void Dispatch(const Frame& frame);

    bool SendFrame(MsgId msgId, const Byte* payload, size_t payloadLen);
    bool SendAll(const Byte* data, size_t len);
    void CloseSocket();

private:
    SOCKET _sock{ INVALID_SOCKET };
    std::thread _thread;
    std::atomic<bool> _running{ false };

    PacketFramer _framer;
    std::mutex _sendMutex;

    std::string _tag;
};
