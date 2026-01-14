#include <iostream>
#include "common/Types.h"
#include "net/PacketFramer.h"

// 프레임 만들어주는 헬퍼: [u16 length][u16 msg_id][payload]
static ByteBuffer MakeFrame(MsgId msgId, const ByteBuffer& payload)
{
    const uint16 length = static_cast<uint16>(2 + payload.size()); // msg_id(2) + payload
    ByteBuffer out;
    out.reserve(2 + length);

    // length LE
    out.push_back(static_cast<Byte>(length & 0xFF));
    out.push_back(static_cast<Byte>((length >> 8) & 0xFF));

    // msg_id LE
    out.push_back(static_cast<Byte>(msgId & 0xFF));
    out.push_back(static_cast<Byte>((msgId >> 8) & 0xFF));

    // payload
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

static void PrintFrame(const Frame& f)
{
    std::cout << "Frame msgId=" << f.msgId << ", payloadLen=" << f.payload.size() << "\n";
}

int main()
{
    PacketFramer framer;

    // TEST 1) 프레임 2개를 붙여서 한 번에 Append -> 2개 Pop
    {
        Frame out;

        ByteBuffer p1 = { 1,2,3 };
        ByteBuffer p2 = { 9,8 };

        auto f1 = MakeFrame(1101, p1);
        auto f2 = MakeFrame(1102, p2);

        ByteBuffer merged;
        merged.reserve(f1.size() + f2.size());
        merged.insert(merged.end(), f1.begin(), f1.end());
        merged.insert(merged.end(), f2.begin(), f2.end());

        if (!framer.Append(merged.data(), merged.size()))
        {
            std::cout << "Append error: " << framer.LastErrorMessage() << "\n";
            return 1;
        }

        auto r1 = framer.TryPopFrame(out);
        if (r1 != PopResult::Ok) { std::cout << "TEST1 pop1 failed\n"; return 1; }
        PrintFrame(out);

        auto r2 = framer.TryPopFrame(out);
        if (r2 != PopResult::Ok) { std::cout << "TEST1 pop2 failed\n"; return 1; }
        PrintFrame(out);

        auto r3 = framer.TryPopFrame(out);
        if (r3 != PopResult::NeedMore) { std::cout << "TEST1 expected NeedMore\n"; return 1; }

        framer.Clear();
        std::cout << "[TEST1 OK]\n";
    }

    // TEST 2) 프레임 1개를 반으로 쪼개서 Append 2번 -> 2번째에 Pop
    {
        Frame out;

        ByteBuffer payload = { 7,7,7,7,7 };
        auto frame = MakeFrame(1200, payload);

        const size_t half = frame.size() / 2;

        if (!framer.Append(frame.data(), half))
        {
            std::cout << "Append error: " << framer.LastErrorMessage() << "\n";
            return 1;
        }

        auto r1 = framer.TryPopFrame(out);
        if (r1 != PopResult::NeedMore) { std::cout << "TEST2 expected NeedMore\n"; return 1; }

        if (!framer.Append(frame.data() + half, frame.size() - half))
        {
            std::cout << "Append error: " << framer.LastErrorMessage() << "\n";
            return 1;
        }

        auto r2 = framer.TryPopFrame(out);
        if (r2 != PopResult::Ok) { std::cout << "TEST2 pop failed\n"; return 1; }
        PrintFrame(out);

        framer.Clear();
        std::cout << "[TEST2 OK]\n";
    }

    std::cout << "All tests passed.\n";
    return 0;
}