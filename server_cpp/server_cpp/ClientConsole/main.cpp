#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "common/ByteIO.h"
#include "common/Types.h"

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

static bool SendPing(SOCKET s, uint32 seq)
{
    ByteWriter w;
    w.WriteU32LE(seq);

    ByteBuffer frame = BuildFrame(/*C_Ping=*/1101, w.buf.data(), w.buf.size());
    return SendAll(s, frame.data(), frame.size());
}

static bool RecvPong(SOCKET s, uint32& outSeq)
{
    // length(2)
    Byte hdr[2];
    if (!RecvExact(s, hdr, 2))
        return false;

    uint16 len = (uint16)hdr[0] | ((uint16)hdr[1] << 8);
    if (len < 2)
        return false;

    // [msgId(2) + payload]
    ByteBuffer rest(len);
    if (!RecvExact(s, rest.data(), rest.size()))
        return false;

    MsgId msgId = (MsgId)rest[0] | ((MsgId)rest[1] << 8);
    if (msgId != /*S_Pong=*/1102)
        return false;

    // payload = u32 seq
    ByteReader br(rest.data() + 2, rest.size() - 2);
    return br.ReadU32LE(outSeq);
}

int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(7777);

    if (::connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << "connect failed: " << WSAGetLastError() << "\n";
        closesocket(s);
        WSACleanup();
        return 1;
    }

    // C_Ping
    uint32 seq = 123;

    if (!SendPing(s, seq))
    {
        std::cout << "SendPing failed\n";
        return 1;
    }

    uint32 pongSeq = 0;
    if (!RecvPong(s, pongSeq))
    {
        std::cout << "RecvPong failed\n";
        return 1;
    }

    std::cout << "Pong received, seq=" << pongSeq << "\n";

    closesocket(s);
    WSACleanup();
}
