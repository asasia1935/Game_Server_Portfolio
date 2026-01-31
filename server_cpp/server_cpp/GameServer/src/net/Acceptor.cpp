#include "net/Acceptor.h"
#include "net/SessionManager.h"
#include "net/Session.h"

#include <iostream>

static void Log(const std::string& tag, const std::string& msg)
{
    std::cout << "[" << tag << "] " << msg << "\n";
}

Acceptor::Acceptor()
{
    _sessionMgr = std::make_unique<SessionManager>();
    _tag = "Acceptor";
}

Acceptor::~Acceptor()
{
    Stop();
}

bool Acceptor::Start(uint16_t port)
{
    if (_running.exchange(true))
        return false;

    _port = port;

    if (!OpenListenSocket(port))
    {
        _running.store(false);
        return false;
    }

    _acceptThread = std::thread(&Acceptor::AcceptLoop, this);
    Log(_tag, "Start listening on port " + std::to_string(port));
    return true;
}

void Acceptor::Stop()
{
    // 멱등
    if (!_running.exchange(false))
        return;

    // accept 깨우기: listen 소켓 닫기
    if (_listenSock != INVALID_SOCKET)
    {
        ::shutdown(_listenSock, SD_BOTH);
        ::closesocket(_listenSock);
        _listenSock = INVALID_SOCKET;
    }

    // accept 스레드 정리
    if (_acceptThread.joinable())
        _acceptThread.join();

    // 전체 세션 종료(Stop=join)
    if (_sessionMgr)
    {
        _sessionMgr->StopAll();
    }

    Log(_tag, "Stopped");
}

bool Acceptor::OpenListenSocket(uint16_t port)
{
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        Log(_tag, "socket() failed");
        return false;
    }

    // 재시작 편의 옵션
    BOOL opt = TRUE;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        Log(_tag, "bind() failed");
        ::closesocket(s);
        return false;
    }

    if (::listen(s, SOMAXCONN) == SOCKET_ERROR)
    {
        Log(_tag, "listen() failed");
        ::closesocket(s);
        return false;
    }

    _listenSock = s;
    return true;
}

void Acceptor::AcceptLoop()
{
    Log(_tag, "AcceptLoop started");

    while (_running.load())
    {
        sockaddr_in caddr{};
        int clen = sizeof(caddr);

        SOCKET clientSock = ::accept(_listenSock, (sockaddr*)&caddr, &clen);
        if (clientSock == INVALID_SOCKET)
        {
            // Stop()에서 listenSock 닫히면 accept가 실패하며 빠져나오게 됨
            break;
        }

        // 세션 생성/등록은 매니저가 담당
        auto session = _sessionMgr->CreateAndAdd(clientSock);
        session->Start();

        char ipbuf[64]{};
        inet_ntop(AF_INET, &caddr.sin_addr, ipbuf, (socklen_t)sizeof(ipbuf));
        uint16_t cport = ntohs(caddr.sin_port);

        Log(_tag, std::string("Accepted client: ") + ipbuf + ":" + std::to_string(cport));
    }

    Log(_tag, "AcceptLoop ended");
}