#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <memory>
#include <thread>
#include <string>

class SessionManager;

class Acceptor
{
public:
    explicit Acceptor(SessionManager* mgr);
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

    SessionManager* _sessionMgr{ nullptr }; // 소유 X, 참조만

    uint16_t _port{ 0 };

    std::string _tag;
};