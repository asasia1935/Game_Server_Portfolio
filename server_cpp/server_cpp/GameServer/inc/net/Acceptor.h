#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <memory>
#include <thread>
#include <string>

class Session;

class Acceptor
{
public:
    Acceptor();
    ~Acceptor();

    // port로 listen 시작 (성공 true)
    bool Start(uint16_t port);

    // listen 중단 + accept 스레드 종료 + 세션 정리
    void Stop();

private:
    void AcceptLoop();
    bool OpenListenSocket(uint16_t port);

private:
    std::atomic<bool> _running{ false };

    SOCKET _listenSock{ INVALID_SOCKET };
    std::thread _acceptThread;

    // 세션매니저 없으니, 단일 세션을 여기서 잡고 lifetime 유지
    std::shared_ptr<Session> _session;

    uint16_t _port{ 0 };

    std::string _tag;
};