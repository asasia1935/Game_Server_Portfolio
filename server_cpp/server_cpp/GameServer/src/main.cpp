#include <iostream>
#include <vector>
#include <string>

#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "common/Types.h"
#include "common/ByteIO.h"
#include "net/Session.h"
#include "net/Acceptor.h"
#include "net/SessionManager.h"

int main()
{
    WSADATA wsa{};
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (ret != 0)
    {
        std::cout << "WSAStartup failed: " << ret << "\n";
        return 1;
    }

    SessionManager sessionMgr;

    // Acceptor가 세션매니저를 쓰게 연결
    Acceptor acceptor(&sessionMgr);
    if (!acceptor.Start(7777))
        return 1;

    std::atomic<bool> reapRun{ true };
    std::thread reaper([&] {
        while (reapRun.load())
        {
            sessionMgr.ReapClosed();
            ::Sleep(50);
        }
        });

    std::cout << "Server listening on 7777\n";
    std::cout << "Press Enter to quit...\n";
    std::cin.get();

    reapRun = false;
    reaper.join();

    acceptor.Stop();
    sessionMgr.StopAll();

    WSACleanup();
    return 0;
}

/////////////////////////////////
// Framer 단위테스트 코드 주석
/////////////////////////////////

//// 프레임 만들어주는 헬퍼: [uint16 length][uint16 msg_id][payload]
//static ByteBuffer MakeFrame(MsgId msgId, const ByteBuffer& payload)
//{
//    const uint16 length = static_cast<uint16>(2 + payload.size()); // msg_id(2) + payload
//    ByteBuffer out;
//    out.reserve(2 + length);
//
//    // length LE
//    out.push_back(static_cast<Byte>(length & 0xFF));
//    out.push_back(static_cast<Byte>((length >> 8) & 0xFF));
//
//    // msg_id LE
//    out.push_back(static_cast<Byte>(msgId & 0xFF));
//    out.push_back(static_cast<Byte>((msgId >> 8) & 0xFF));
//
//    // payload
//    out.insert(out.end(), payload.begin(), payload.end());
//    return out;
//}
//
//static void PrintFrame(const Frame& f)
//{
//    std::cout << "Frame msgId=" << f.msgId << ", payloadLen=" << f.payload.size() << "\n";
//}
//PacketFramer framer;
//
//// TEST 1) 프레임 2개를 붙여서 한 번에 Append -> 2개 Pop
//{
//    Frame out;
//
//    ByteBuffer p1 = { 1,2,3 };
//    ByteBuffer p2 = { 9,8 };
//
//    auto f1 = MakeFrame(1101, p1);
//    auto f2 = MakeFrame(1102, p2);
//
//    ByteBuffer merged;
//    merged.reserve(f1.size() + f2.size());
//    merged.insert(merged.end(), f1.begin(), f1.end());
//    merged.insert(merged.end(), f2.begin(), f2.end());
//
//    if (!framer.Append(merged.data(), merged.size()))
//    {
//        std::cout << "Append error: " << framer.LastErrorMessage() << "\n";
//        return 1;
//    }
//
//    auto r1 = framer.TryPopFrame(out);
//    if (r1 != PopResult::Ok) { std::cout << "TEST1 pop1 failed\n"; return 1; }
//    PrintFrame(out);
//
//    auto r2 = framer.TryPopFrame(out);
//    if (r2 != PopResult::Ok) { std::cout << "TEST1 pop2 failed\n"; return 1; }
//    PrintFrame(out);
//
//    auto r3 = framer.TryPopFrame(out);
//    if (r3 != PopResult::NeedMore) { std::cout << "TEST1 expected NeedMore\n"; return 1; }
//
//    framer.Clear();
//    std::cout << "[TEST1 OK]\n";
//}
//
//// TEST 2) 프레임 1개를 반으로 쪼개서 Append 2번 -> 2번째에 Pop
//{
//    Frame out;
//
//    ByteBuffer payload = { 7,7,7,7,7 };
//    auto frame = MakeFrame(1200, payload);
//
//    const size_t half = frame.size() / 2;
//
//    if (!framer.Append(frame.data(), half))
//    {
//        std::cout << "Append error: " << framer.LastErrorMessage() << "\n";
//        return 1;
//    }
//
//    auto r1 = framer.TryPopFrame(out);
//    if (r1 != PopResult::NeedMore) { std::cout << "TEST2 expected NeedMore\n"; return 1; }
//
//    if (!framer.Append(frame.data() + half, frame.size() - half))
//    {
//        std::cout << "Append error: " << framer.LastErrorMessage() << "\n";
//        return 1;
//    }
//
//    auto r2 = framer.TryPopFrame(out);
//    if (r2 != PopResult::Ok) { std::cout << "TEST2 pop failed\n"; return 1; }
//    PrintFrame(out);
//
//    framer.Clear();
//    std::cout << "[TEST2 OK]\n";
//}
//
//std::cout << "All tests passed.\n";

//////////////////////////////////////////////////////////////////

/////////////////////////////////
// Session 단위테스트 코드 주석
/////////////////////////////////

/*
// ---- (HELPERS) -------------------------------------------------------------

static bool SendAll(SOCKET s, const Byte* data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        int n = ::send(s, (const char*)(data + sent), (int)(len - sent), 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

static bool RecvExact(SOCKET s, Byte* out, size_t len)
{
    size_t got = 0;
    while (got < len)
    {
        int n = ::recv(s, (char*)(out + got), (int)(len - got), 0);
        if (n <= 0) return false;
        got += (size_t)n;
    }
    return true;
}

static bool MakeLoopbackConnection(SOCKET& outServerAccepted, SOCKET& outClient)
{
    outServerAccepted = INVALID_SOCKET;
    outClient = INVALID_SOCKET;

    SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) return false;

    // bind to 127.0.0.1:0 (port 0 => OS assigns)
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(0);

    if (::bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        ::closesocket(listenSock);
        return false;
    }

    if (::listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        ::closesocket(listenSock);
        return false;
    }

    // get assigned port
    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    if (::getsockname(listenSock, (sockaddr*)&bound, &boundLen) == SOCKET_ERROR)
    {
        ::closesocket(listenSock);
        return false;
    }
    unsigned short port = ntohs(bound.sin_port);

    // create client and connect
    SOCKET clientSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET)
    {
        ::closesocket(listenSock);
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    if (::connect(clientSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        ::closesocket(clientSock);
        ::closesocket(listenSock);
        return false;
    }

    // accept on server
    sockaddr_in peer{};
    int peerLen = sizeof(peer);
    SOCKET accepted = ::accept(listenSock, (sockaddr*)&peer, &peerLen);

    ::closesocket(listenSock);

    if (accepted == INVALID_SOCKET)
    {
        ::closesocket(clientSock);
        return false;
    }

    outServerAccepted = accepted;
    outClient = clientSock;
    return true;
}

// ---- MAIN TEST --------------------------------------------------------------

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // Session unit test: Ping -> Pong
    // -----------------------------------------------------------------------
    SOCKET serverSock = INVALID_SOCKET;
    SOCKET clientSock = INVALID_SOCKET;

    if (!MakeLoopbackConnection(serverSock, clientSock))
    {
        std::cout << "MakeLoopbackConnection failed\n";
        WSACleanup();
        return 1;
    }

    // 서버 측: Session은 accept로 받은 소켓으로 시작
    Session session(serverSock);
    session.Start();

    // 클라 측: C_Ping(seq) 프레임 전송
    static constexpr MsgId C_Ping = 1101;
    static constexpr MsgId S_Pong = 1102;

    const uint32 seq = 123456;

    ByteWriter w;
    w.WriteU32LE(seq);

    ByteBuffer pingFrame = BuildFrame(C_Ping, w.buf.data(), w.buf.size());
    if (!SendAll(clientSock, pingFrame.data(), pingFrame.size()))
    {
        std::cout << "Client SendAll failed\n";
        ::closesocket(clientSock);
        session.Stop();
        WSACleanup();
        return 1;
    }

    // 클라 측: 응답 프레임 수신
    // 먼저 length(2) 읽고, 그 다음 length만큼 읽기
    Byte hdr[2];
    if (!RecvExact(clientSock, hdr, 2))
    {
        std::cout << "Client Recv length failed\n";
        ::closesocket(clientSock);
        session.Stop();
        WSACleanup();
        return 1;
    }

    const uint16 length = (uint16)hdr[0] | ((uint16)hdr[1] << 8);
    if (length < 2 || (size_t)(2 + length) > MAX_FRAME_TOTAL)
    {
        std::cout << "Malformed length from server: " << length << "\n";
        ::closesocket(clientSock);
        session.Stop();
        WSACleanup();
        return 1;
    }

    ByteBuffer rest;
    rest.resize(length);
    if (!RecvExact(clientSock, rest.data(), rest.size()))
    {
        std::cout << "Client Recv rest failed\n";
        ::closesocket(clientSock);
        session.Stop();
        WSACleanup();
        return 1;
    }

    // rest = [msg_id(2)][payload...]
    const MsgId msgId = (MsgId)rest[0] | ((MsgId)rest[1] << 8);
    if (msgId != S_Pong)
    {
        std::cout << "Expected S_Pong(" << S_Pong << "), got " << msgId << "\n";
        ::closesocket(clientSock);
        session.Stop();
        WSACleanup();
        return 1;
    }

    // payload = u32 seq
    if (rest.size() != 2 + 4)
    {
        std::cout << "Unexpected payload size: " << (rest.size() - 2) << "\n";
        ::closesocket(clientSock);
        session.Stop();
        WSACleanup();
        return 1;
    }

    ByteReader br(rest.data() + 2, rest.size() - 2);
    uint32 gotSeq = 0;
    if (!br.ReadU32LE(gotSeq) || gotSeq != seq)
    {
        std::cout << "Seq mismatch. expected=" << seq << " got=" << gotSeq << "\n";
        ::closesocket(clientSock);
        session.Stop();
        WSACleanup();
        return 1;
    }

    std::cout << "[SESSION TEST OK] Ping/Pong seq=" << gotSeq << "\n";

    ::closesocket(clientSock);
    session.Stop();

    WSACleanup();
    return 0;
*/